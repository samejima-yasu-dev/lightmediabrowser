#include "ui/MainWindow.h"
#include "ui/VisibleThumbnailRows.h"
#include "ui/VideoListLayout.h"
#include "ui/TagManager.h"
#include "media/VideoMetadataProbe.h"
#include "media/MediaToolLocator.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QIcon>
#include <QInputDialog>
#include <QMap>
#include <QSettings>
#include <QDesktopServices>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPalette>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QtConcurrent/QtConcurrentRun>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QString error;
    bool databaseCreated = false;
    if (!m_database.open(&error, &databaseCreated)) {
        qWarning() << "Database initialization failed:" << error;
        QMessageBox::critical(this, QStringLiteral("Database Error"), error);
    } else {
        m_databaseReady = true;
        if (databaseCreated) {
            QSettings settings;
            const QString foldersKey = QStringLiteral("media/folders");
            if (settings.contains(foldersKey)) {
                settings.setValue(foldersKey, QStringList{});
            }
        }
    }
    buildUi();
    const QString mediaToolsMessage = missingMediaToolsMessage(locateMediaTools());
    if (!mediaToolsMessage.isEmpty()) {
        statusBar()->showMessage(mediaToolsMessage, 10000);
    }
    m_tagManager = new TagManager(&m_database, m_tagTree, this);
    connect(m_tagManager, &TagManager::tagSelectionChanged, this,
            [this](qint64 tagId) { m_selectedTagId = tagId; refreshVideos(); });
    connect(m_tagManager, &TagManager::libraryChanged, this,
            [this] { refreshTags(); refreshVideos(); });
    loadFolders();
    refreshVideos();
    refreshTags();
    connect(&m_scanner, &MediaScanner::videosDiscovered, this,
            [this](const QList<Video> &videos) {
        if (!m_databaseReady || videos.isEmpty()) return;
        QString error;
        if (!m_database.upsertVideos(videos, &error)) {
            qWarning() << "Could not register scanned videos:" << error;
            statusBar()->showMessage(error);
            return;
        }
    });
    connect(&m_scanner, &MediaScanner::finished, this, &MainWindow::showScanResult);
    connect(&m_scanner, &MediaScanner::progress, this, [this](int count) {
        statusBar()->showMessage(QStringLiteral("Scanning... %1 videos found").arg(count));
    });
    connect(&m_thumbnailService, &ThumbnailService::generated, this, [this](const QString &videoPath, const QString &thumbnailPath) {
        m_thumbnailPaths.insert(videoPath, thumbnailPath);
        QListWidgetItem *item = m_videoItemsByPath.value(videoPath, nullptr);
        if (item) {
            item->setIcon(QIcon(thumbnailPath));
            m_thumbnailDispatchTimer->start();
        }
    });
    connect(&m_thumbnailService, &ThumbnailService::failed, this, [this](const QString &videoPath, const QString &message) {
        QListWidgetItem *item = m_videoItemsByPath.value(videoPath, nullptr);
        if (item) item->setIcon(QIcon());
        if (item && !m_bulkThumbnailRunning) {
            statusBar()->showMessage(message.isEmpty()
                ? QStringLiteral("Thumbnail generation failed")
                : QStringLiteral("Thumbnail failed: %1").arg(message));
        }
    });
    connect(&m_thumbnailService, &ThumbnailService::sampleGenerated, this,
            [this](const QString &videoPath, int sampleIndex, const QString &thumbnailPath) {
        if (videoPath != m_selectedVideoPath || sampleIndex < 0
            || sampleIndex >= m_detailSampleLabels.size()) {
            return;
        }
        const QPixmap thumbnail(thumbnailPath);
        if (thumbnail.isNull()) return;
        m_detailSampleLabels.at(sampleIndex)->setPixmap(thumbnail);
        m_detailSampleLabels.at(sampleIndex)->setToolTip(thumbnailPath);
    });
    connect(&m_thumbnailService, &ThumbnailService::sampleFailed, this,
            [this](const QString &videoPath, int sampleIndex, const QString &message) {
        if (videoPath != m_selectedVideoPath || sampleIndex < 0
            || sampleIndex >= m_detailSampleLabels.size()) {
            return;
        }
        m_detailSampleLabels.at(sampleIndex)->setText(QStringLiteral("Preview unavailable"));
        statusBar()->showMessage(QStringLiteral("Scene preview failed: %1").arg(message));
    });
    if (!m_currentFolder.isEmpty()) QTimer::singleShot(0, this, &MainWindow::scanFolder);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("LightMediaBrowser v0.0.16"));
    showMaximized(); // アプリ起動時にウィンドウを最大化（フルスクリーン表示）してボタンが隠れないようにする
    const QString lightStyle = QStringLiteral(R"(
        QMainWindow, QWidget { background: #f5f7f8; color: #1d2930; }
        QToolBar { background: #ffffff; border: 0; border-bottom: 1px solid #d7e0e3; spacing: 8px; padding: 9px 12px; }
        QToolButton { color: #1d2930; background: #ffffff; border: 1px solid #b9c8cd; border-radius: 6px; padding: 7px 10px; }
        QToolButton:hover { background: #eaf4f2; border-color: #2a9d8f; }
        QToolButton:pressed { background: #2a9d8f; border-color: #237f74; color: #ffffff; }
        QComboBox, QLineEdit { background: #ffffff; border: 1px solid #b9c8cd; border-radius: 6px; padding: 8px 10px; color: #1d2930; selection-background-color: #b8e0d8; }
        QComboBox:hover, QLineEdit:focus { border-color: #2a9d8f; }
        QSplitter::handle { background: #d7e0e3; }
        QTreeWidget { background: #ffffff; border: 0; padding: 10px 7px; outline: 0; }
        QTreeWidget::item { height: 32px; padding: 4px 8px; border-radius: 5px; }
        QTreeWidget::item:hover { background: #edf5f4; }
        QTreeWidget::item:selected { background: #2a9d8f; color: #ffffff; }
        QHeaderView::section { background: #ffffff; color: #52636a; border: 0; padding: 8px; font-weight: 600; }
        QListWidget { background: #f5f7f8; border: 0; padding: %1px; outline: 0; }
        QListWidget::item { background: #ffffff; border: 1px solid #d7e0e3; border-radius: 8px; color: #1d2930; padding: 8px; }
        QListWidget::item:hover { background: #edf5f4; border-color: #2a9d8f; }
        QListWidget::item:selected { background: #d8eee9; border: 2px solid #2a9d8f; color: #1d2930; }
        QWidget#detailsPanel { background: #ffffff; border-left: 1px solid #d7e0e3; color: #33454d; }
        QLabel#sceneThumbnail { background: #edf1f2; border: 1px solid #d7e0e3; color: #52636a; }
        QScrollArea#detailsScroll { border: 0; }
        QStatusBar { background: #ffffff; color: #52636a; border-top: 1px solid #d7e0e3; }
    )").arg(VideoListVerticalPadding);
    const QString darkStyle = QStringLiteral(R"(
        QMainWindow, QWidget { background: #171a1d; color: #e7eceb; }
        QToolBar { background: #202529; border-bottom-color: #343b3f; }
        QToolButton { color: #f2f6f5; background: #2b3335; border-color: #465354; }
        QToolButton:hover { background: #30393b; border-color: #43c6a8; }
        QToolButton:pressed { background: #1f8a78; border-color: #43c6a8; color: #ffffff; }
        QComboBox, QLineEdit { background: #121517; border-color: #3a4546; color: #f2f6f5; selection-background-color: #1f8a78; }
        QComboBox:hover, QLineEdit:focus { border-color: #43c6a8; }
        QSplitter::handle { background: #343b3f; }
        QTreeWidget, QHeaderView::section { background: #1d2225; }
        QTreeWidget::item:hover { background: #2b3435; }
        QTreeWidget::item:selected { background: #1f8a78; color: #ffffff; }
        QHeaderView::section { color: #9caead; }
        QListWidget { background: #121517; }
        QListWidget::item { background: #242a2d; border-color: #343d3f; color: #e7eceb; }
        QListWidget::item:hover { background: #2c3536; border-color: #43c6a8; }
        QListWidget::item:selected { background: #23443e; border-color: #43c6a8; color: #ffffff; }
        QWidget#detailsPanel { background: #1d2225; border-left-color: #343b3f; color: #dce5e3; }
        QLabel#sceneThumbnail { background: #242a2d; border-color: #343d3f; color: #9caead; }
        QStatusBar { background: #202529; color: #9caead; border-top-color: #343b3f; }
    )");
    // 起動時にデフォルトでダークモードを適用（QSettingsに保存がない場合はtrue=ダークをデフォルトとする）
    const bool defaultDark = QSettings().value(QStringLiteral("ui/darkTheme"), true).toBool();
    setStyleSheet(defaultDark ? lightStyle + darkStyle : lightStyle);

    auto *toolbar = addToolBar(QStringLiteral("Library"));

    auto *addAction = toolbar->addAction(QStringLiteral("＋ Add Folder"));
    addAction->setToolTip(QStringLiteral("Add media folder"));
    auto *removeAction = toolbar->addAction(QStringLiteral("－ Remove"));
    removeAction->setToolTip(QStringLiteral("Remove current folder"));
    auto *scanAction = toolbar->addAction(QStringLiteral("🔄 Scan"));
    scanAction->setToolTip(QStringLiteral("Scan all registered folders for videos"));

    m_folderSelector = new QComboBox(this);
    m_folderSelector->setObjectName(QStringLiteral("folderCombo"));
    m_folderSelector->setMinimumWidth(220);
    toolbar->addWidget(m_folderSelector);

    toolbar->addSeparator();

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("searchLineEdit"));
    m_search->setPlaceholderText(QStringLiteral("🔍 Search videos..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(200);
    toolbar->addWidget(m_search);

    m_libraryFilter = new QComboBox(this);
    m_libraryFilter->setObjectName(QStringLiteral("libraryFilterCombo"));
    m_libraryFilter->addItem(QStringLiteral("All Videos"), false);
    m_libraryFilter->addItem(QStringLiteral("★ Favorites"), true);
    toolbar->addWidget(m_libraryFilter);

    m_duplicateFilter = new QComboBox(this);
    m_duplicateFilter->setObjectName(QStringLiteral("duplicateFilterCombo"));
    m_duplicateFilter->addItem(QStringLiteral("All"), false);
    m_duplicateFilter->addItem(QStringLiteral("Same size or filename"), true);
    m_duplicateFilter->setToolTip(QStringLiteral(
        "Show files whose size or filename matches another library file. "
        "Matching either value does not confirm identical contents."));
    toolbar->addWidget(m_duplicateFilter);

    toolbar->addSeparator();

    m_sortSelector = new QComboBox(this);
    m_sortSelector->setObjectName(QStringLiteral("sortSelectorCombo"));
    m_sortSelector->addItem(QStringLiteral("Newest"), QStringLiteral("newest"));
    m_sortSelector->addItem(QStringLiteral("Name"), QStringLiteral("name"));
    m_sortSelector->addItem(QStringLiteral("Size"), QStringLiteral("size"));
    m_sortSelector->addItem(QStringLiteral("Most Played"), QStringLiteral("plays"));
    toolbar->addWidget(m_sortSelector);

    m_minPlayCount = new QSpinBox(this);
    m_minPlayCount->setObjectName(QStringLiteral("minPlayCountSpin"));
    m_minPlayCount->setRange(0, 1000000);
    m_minPlayCount->setPrefix(QStringLiteral("Plays ≥ "));
    m_minPlayCount->setToolTip(QStringLiteral("Minimum play count"));
    toolbar->addWidget(m_minPlayCount);

    toolbar->addSeparator();

    auto *favoriteAction = toolbar->addAction(QStringLiteral("★ Favorite"));
    m_generateAllThumbnailsAction = toolbar->addAction(
        QStringLiteral("Generate All Thumbnails"));
    m_generateAllThumbnailsAction->setToolTip(QStringLiteral(
        "Generate missing thumbnails for every registered video using two workers."));
    const bool limitThumbnailThreads = QSettings()
        .value(QStringLiteral("media/limitThumbnailThreads"), true).toBool();
    m_thumbnailService.setThreadLimitEnabled(limitThumbnailThreads);
    auto *thumbnailLimitAction = toolbar->addAction(
        limitThumbnailThreads
            ? QStringLiteral("Thumbnail Threads: Limited")
            : QStringLiteral("Thumbnail Threads: Unlimited"));
    thumbnailLimitAction->setCheckable(true);
    thumbnailLimitAction->setChecked(limitThumbnailThreads);
    thumbnailLimitAction->setToolTip(QStringLiteral(
        "Toggle the parallel thumbnail generation thread limit."));
    auto *themeAction = toolbar->addAction(QStringLiteral("🌓 Theme"));
    themeAction->setCheckable(true);
    themeAction->setChecked(defaultDark); // 起動時の設定状態に合わせる
    themeAction->setToolTip(QStringLiteral("Switch between Dark and Light theme"));
    connect(addAction, &QAction::triggered, this, &MainWindow::addFolder);
    connect(removeAction, &QAction::triggered, this, &MainWindow::removeFolder);
    connect(scanAction, &QAction::triggered, this, &MainWindow::scanFolder);
    connect(favoriteAction, &QAction::triggered, this, &MainWindow::toggleFavorite);
    connect(m_generateAllThumbnailsAction, &QAction::triggered,
            this, &MainWindow::generateAllThumbnails);
    connect(thumbnailLimitAction, &QAction::toggled, this, [this, thumbnailLimitAction](bool enabled) {
        QSettings().setValue(QStringLiteral("media/limitThumbnailThreads"), enabled);
        m_thumbnailService.setThreadLimitEnabled(enabled);
        thumbnailLimitAction->setText(enabled
            ? QStringLiteral("Thumbnail Threads: Limited")
            : QStringLiteral("Thumbnail Threads: Unlimited"));
    });
    connect(themeAction, &QAction::toggled, this, [this, lightStyle, darkStyle](bool dark) {
        QSettings().setValue(QStringLiteral("ui/darkTheme"), dark);
        setStyleSheet(dark ? lightStyle + darkStyle : lightStyle);
    });
    connect(m_folderSelector, &QComboBox::currentTextChanged, this, [this](const QString &folder) {
        m_currentFolder = folder;
    });

    // 検索バーへの文字入力ごとに即座に重い処理が走るのを防ぐため、デバウンス用タイマーを導入
    QTimer *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(250); // 0.25秒間入力を待ってから検索を実行
    connect(m_search, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));
    connect(searchTimer, &QTimer::timeout, this, &MainWindow::refreshVideos);

    connect(m_libraryFilter, &QComboBox::currentIndexChanged, this, &MainWindow::refreshVideos);
    connect(m_duplicateFilter, &QComboBox::currentIndexChanged, this, &MainWindow::refreshVideos);
    connect(m_sortSelector, &QComboBox::currentIndexChanged, this, &MainWindow::refreshVideos);
    connect(m_minPlayCount, &QSpinBox::valueChanged, this, &MainWindow::refreshVideos);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_tagTree = new QTreeWidget(splitter);
    m_tagTree->setObjectName(QStringLiteral("tagTree"));
    m_tagTree->setHeaderLabels({QStringLiteral("Tags")});
    m_tagTree->setMinimumWidth(190);
    m_tagTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_videoList = new QListWidget(splitter);
    m_videoList->setObjectName(QStringLiteral("videoList"));
    m_videoList->setViewMode(QListView::IconMode);
    m_videoList->setResizeMode(QListView::Adjust);
    m_videoList->setMovement(QListView::Static);
    m_videoList->setUniformItemSizes(true);
    m_videoList->setIconSize(QSize(200, 112));
    const QSize videoGridSize = videoListGridSize(m_videoList->iconSize(),
                                                  m_videoList->fontMetrics().height());
    m_videoList->setGridSize(videoGridSize);
    m_videoList->setMinimumHeight(videoListMinimumHeight(videoGridSize));
    m_videoList->setSpacing(12);
    configureVideoListScrolling(*m_videoList);
    m_videoList->setContextMenuPolicy(Qt::CustomContextMenu);
    auto *detailsScroll = new QScrollArea(splitter);
    detailsScroll->setObjectName(QStringLiteral("detailsScroll"));
    detailsScroll->setWidgetResizable(true);
    detailsScroll->setMinimumWidth(270);
    auto *detailsPanel = new QWidget(detailsScroll);
    detailsPanel->setObjectName(QStringLiteral("detailsPanel"));
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    m_details = new QLabel(detailsPanel);
    m_details->setText(QStringLiteral("Select a video to view file info and tags."));
    m_details->setWordWrap(true);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailsLayout->addWidget(m_details);
    auto *previewHeading = new QLabel(QStringLiteral("Scene previews"), detailsPanel);
    previewHeading->setStyleSheet(QStringLiteral("font-weight: 600;"));
    detailsLayout->addWidget(previewHeading);
    auto *previewGrid = new QGridLayout();
    previewGrid->setContentsMargins(0, 0, 0, 0);
    previewGrid->setVerticalSpacing(10);
    for (int index = 0; index < 3; ++index) {
        auto *thumbnail = new QLabel(QStringLiteral("Select a video"), detailsPanel);
        thumbnail->setObjectName(QStringLiteral("sceneThumbnail"));
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setFixedSize(220, 124);
        thumbnail->setScaledContents(true);
        m_detailSampleLabels.append(thumbnail);
        previewGrid->addWidget(thumbnail, index, 0);
    }
    detailsLayout->addLayout(previewGrid);
    detailsLayout->addStretch();
    detailsScroll->setWidget(detailsPanel);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    setCentralWidget(splitter);

    auto *detailsTimer = new QTimer(this);
    detailsTimer->setSingleShot(true);
    detailsTimer->setInterval(200);
    connect(detailsTimer, &QTimer::timeout, this, [this] {
        showDetails(m_videoList->currentItem());
    });
    connect(m_videoList, &QListWidget::currentItemChanged, this, [this, detailsTimer](QListWidgetItem *current, QListWidgetItem *) {
        detailsTimer->start();
        const int itemsPerRow = qMax(1, m_videoList->viewport()->width() / m_videoList->gridSize().width());
        if (current && m_loadedVideoCount > 0
            && m_videoList->row(current) >= m_loadedVideoCount - itemsPerRow) {
            loadMoreVideos();
        }
    });
    connect(m_videoList, &QListWidget::itemActivated, this, &MainWindow::playVideo);
    connect(m_videoList, &QListWidget::customContextMenuRequested, this, &MainWindow::showVideoContextMenu);
    connect(m_tagTree, &QTreeWidget::itemClicked, this, &MainWindow::selectTag);
    connect(m_tagTree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTagContextMenu);

    m_thumbnailDispatchTimer = new QTimer(this);
    m_thumbnailDispatchTimer->setSingleShot(true);
    m_thumbnailDispatchTimer->setInterval(100);
    connect(m_thumbnailDispatchTimer, &QTimer::timeout,
            this, &MainWindow::generateVisibleThumbnails);

    // 無限スクロール・遅延ロード対応：スクロールバーが一番下付近に達したら追加ロードする
    connect(m_videoList->verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this](int value) {
        auto *bar = m_videoList->verticalScrollBar();
        m_thumbnailDispatchTimer->start();
        // スクロール位置が最大値の100px手前以内に達した場合に次のページをロード
        if (bar && value >= bar->maximum() - 100) {
            loadMoreVideos();
        }
    });

    m_bulkThumbnailStatus = new QLabel(this);
    m_bulkThumbnailProgress = new QProgressBar(this);
    m_bulkThumbnailProgress->setTextVisible(false);
    m_bulkThumbnailProgress->setFixedWidth(150);
    m_cancelBulkThumbnailButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelBulkThumbnailButton->setFlat(true);
    statusBar()->addPermanentWidget(m_bulkThumbnailStatus, 1);
    statusBar()->addPermanentWidget(m_bulkThumbnailProgress);
    statusBar()->addPermanentWidget(m_cancelBulkThumbnailButton);
    m_bulkThumbnailStatus->hide();
    m_bulkThumbnailProgress->hide();
    m_cancelBulkThumbnailButton->hide();
    connect(m_cancelBulkThumbnailButton, &QPushButton::clicked, this, [this] {
        if (!m_bulkThumbnailRunning) return;
        m_thumbnailService.cancelGenerateAll();
        m_cancelBulkThumbnailButton->setText(QStringLiteral("Canceling..."));
        m_cancelBulkThumbnailButton->setEnabled(false);
    });

    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::generateAllThumbnails()
{
    if (!m_databaseReady) {
        statusBar()->showMessage(QStringLiteral("Database is not available"));
        return;
    }

    const QList<VideoLightItem> videos = m_database.searchVideosLight(
        {}, 0, false, 0, false, QStringLiteral("name"), -1, 0);
    QStringList videoPaths;
    videoPaths.reserve(videos.size());
    for (const VideoLightItem &video : videos) {
        videoPaths.append(video.filePath);
    }
    if (videoPaths.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("No videos to generate thumbnails for"));
        return;
    }

    m_bulkThumbnailProgress->setRange(0, videoPaths.size());
    m_bulkThumbnailProgress->setValue(0);
    m_bulkThumbnailStatus->setText(QStringLiteral("Preparing %1 thumbnails...")
                                       .arg(videoPaths.size()));
    m_cancelBulkThumbnailButton->setText(QStringLiteral("Cancel"));
    m_cancelBulkThumbnailButton->setEnabled(true);
    m_bulkThumbnailStatus->show();
    m_bulkThumbnailProgress->show();
    m_cancelBulkThumbnailButton->show();

    auto *bulkUiContext = new QObject(this);
    connect(
        &m_thumbnailService, &ThumbnailService::bulkProgress, bulkUiContext,
        [this](int completed, int total, int generated, int skipped, int failed) {
            m_bulkThumbnailProgress->setRange(0, qMax(1, total));
            m_bulkThumbnailProgress->setValue(completed);
            m_bulkThumbnailStatus->setText(QStringLiteral(
                "Thumbnails %1/%2 — %3 generated, %4 existing, %5 failed")
                                               .arg(completed).arg(total)
                                               .arg(generated).arg(skipped).arg(failed));
        });
    connect(
        &m_thumbnailService, &ThumbnailService::bulkFinished, bulkUiContext,
        [this, bulkUiContext](int, int generated, int skipped, int failed, bool canceled) {
            m_bulkThumbnailRunning = false;
            m_bulkThumbnailStatus->hide();
            m_bulkThumbnailProgress->hide();
            m_cancelBulkThumbnailButton->hide();
            m_cancelBulkThumbnailButton->setText(QStringLiteral("Cancel"));
            m_cancelBulkThumbnailButton->setEnabled(true);
            m_generateAllThumbnailsAction->setEnabled(true);
            statusBar()->showMessage(
                QStringLiteral("%1 Thumbnail generation: %2 generated, %3 existing, %4 failed")
                    .arg(canceled ? QStringLiteral("Canceled.") : QStringLiteral("Finished."))
                    .arg(generated).arg(skipped).arg(failed));
            QObject::disconnect(&m_thumbnailService, nullptr, bulkUiContext, nullptr);
            bulkUiContext->deleteLater();
        });

    m_generateAllThumbnailsAction->setEnabled(false);
    m_bulkThumbnailRunning = true;
    if (!m_thumbnailService.generateAll(videoPaths)) {
        m_bulkThumbnailRunning = false;
        QObject::disconnect(&m_thumbnailService, nullptr, bulkUiContext, nullptr);
        bulkUiContext->deleteLater();
        m_generateAllThumbnailsAction->setEnabled(true);
        m_bulkThumbnailStatus->hide();
        m_bulkThumbnailProgress->hide();
        m_cancelBulkThumbnailButton->hide();
        statusBar()->showMessage(QStringLiteral("Thumbnail generation is already running"));
        return;
    }
}

void MainWindow::addFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Add media folder"));
    if (folder.isEmpty()) return;
    m_currentFolder = folder;
    if (!m_folders.contains(folder)) {
        m_folders.append(folder);
        m_folderSelector->addItem(folder);
        saveFolders();
    }
    m_folderSelector->setCurrentText(folder);
    statusBar()->showMessage(QStringLiteral("Folder added: %1").arg(folder));
    scanFolder();
}

void MainWindow::removeFolder()
{
    const QString folder = m_folderSelector ? m_folderSelector->currentText() : QString();
    if (folder.isEmpty()) return;
    m_folders.removeAll(folder);
    m_folderSelector->removeItem(m_folderSelector->currentIndex());
    m_currentFolder = m_folderSelector->currentText();
    saveFolders();
    statusBar()->showMessage(QStringLiteral("Folder removed: %1").arg(folder));
}

void MainWindow::scanFolder()
{
    if (m_folders.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("No media folders registered. Use Add Folder first."));
        return;
    }
    qInfo() << "Starting scan across" << m_folders.size() << "registered folders";
    statusBar()->showMessage(QStringLiteral("Scanning all folders..."));
    m_scanner.scanFolders(m_folders);
}

LibraryFilterState MainWindow::currentLibraryFilterState() const
{
    LibraryFilterState state;
    state.query = m_search ? m_search->text() : QString();
    state.tagId = m_selectedTagId;
    state.favoritesOnly = m_libraryFilter && m_libraryFilter->currentData().toBool();
    state.duplicatesOnly = m_duplicateFilter && m_duplicateFilter->currentData().toBool();
    state.minPlayCount = m_minPlayCount ? m_minPlayCount->value() : 0;
    state.sortKey = m_sortSelector ? m_sortSelector->currentData().toString() : QStringLiteral("name");
    return state;
}

void MainWindow::refreshVideos()
{
    m_videoList->clear();
    m_videoItemsByPath.clear();
    m_loadedVideoCount = 0;
    m_nextVideoOffset = 0;
    m_totalVideoCount = 0;
    m_pageLoadScheduled = false;
    ++m_listingGeneration;
    if (!m_databaseReady) return;

    m_filterState = currentLibraryFilterState();
    m_totalVideoCount = m_database.countVideosLight(
        m_filterState.query, m_filterState.tagId, m_filterState.favoritesOnly,
        m_filterState.minPlayCount, m_filterState.duplicatesOnly);
    loadMoreVideos();
}

void MainWindow::loadMoreVideos()
{
    if (!m_databaseReady || m_nextVideoOffset >= m_totalVideoCount) return;

    const int targetCount = m_loadedVideoCount + m_pageSize;
    const QList<VideoLightItem> page = m_database.searchVideosLight(
        m_filterState.query, m_filterState.tagId, m_filterState.favoritesOnly,
        m_filterState.minPlayCount, m_filterState.duplicatesOnly,
        m_filterState.normalizedSortKey(), m_pageSize, m_nextVideoOffset);
    m_nextVideoOffset += page.size();
    for (const VideoLightItem &video : page) {
        const QFileInfo fileInfo(video.filePath);
        if (!fileInfo.exists() || !QDir(fileInfo.absolutePath()).exists()) continue;

        const QString marker = video.favorite ? QStringLiteral("★ ") : QString();
        auto *item = new QListWidgetItem(
            marker + video.fileName + QStringLiteral("\nPlays: ") + QString::number(video.playCount), m_videoList);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setSizeHint(m_videoList->gridSize());
        item->setData(Qt::UserRole, video.id);
        item->setToolTip(video.filePath);
        m_videoItemsByPath.insert(video.filePath, item);
        const QString thumbnailPath = m_thumbnailPaths.value(video.filePath, ThumbnailService::cachedPath(video.filePath));
        if (QFileInfo::exists(thumbnailPath)) item->setIcon(QIcon(thumbnailPath));
        ++m_loadedVideoCount;
    }
    m_thumbnailDispatchTimer->start();
    if (m_loadedVideoCount < targetCount && m_nextVideoOffset < m_totalVideoCount &&
        !page.isEmpty() && !m_pageLoadScheduled) {
        m_pageLoadScheduled = true;
        const quint64 generation = m_listingGeneration;
        QTimer::singleShot(0, this, [this, generation] {
            if (generation != m_listingGeneration) return;
            m_pageLoadScheduled = false;
            loadMoreVideos();
        });
    }
    statusBar()->showMessage(QStringLiteral("%1 / %2 videos displayed").arg(m_loadedVideoCount).arg(m_totalVideoCount));
}

void MainWindow::generateVisibleThumbnails()
{
    const QRect viewport = m_videoList->viewport()->rect();
    int firstRow = m_videoList->indexAt(QPoint(0, 0)).row();
    if (firstRow < 0) firstRow = 0;

    QList<QRect> itemRects;
    for (int row = firstRow; row < m_videoList->count(); ++row) {
        const QRect itemRect = m_videoList->visualItemRect(m_videoList->item(row));
        if (!itemRect.isValid() || itemRect.top() > viewport.bottom()) break;
        itemRects.append(itemRect);
    }

    for (const int visibleIndex : visibleThumbnailRows(itemRects, viewport)) {
        const QListWidgetItem *item = m_videoList->item(firstRow + visibleIndex);
        m_thumbnailService.generate(item->toolTip());
    }
}

void MainWindow::showVideoContextMenu(const QPoint &position)
{
    auto *item = m_videoList->itemAt(position);
    if (!item) return;
    m_videoList->setCurrentItem(item);
    QMenu menu(this);
    menu.addAction(QStringLiteral("▶ Play"), this, [this, item] { playVideo(item); });
    menu.addAction(QStringLiteral("★ Toggle Favorite"), this, &MainWindow::toggleFavorite);
    menu.addSeparator();
    menu.addAction(QStringLiteral("🏷️ Assign Tags..."), this, &MainWindow::assignTagsToVideo);
    menu.exec(m_videoList->viewport()->mapToGlobal(position));
}

void MainWindow::showTagContextMenu(const QPoint &position)
{
    if (m_tagManager) m_tagManager->showContextMenu(position);
}

void MainWindow::assignTagsToVideo()
{
    auto *item = m_videoList->currentItem();
    if (!item || !m_tagManager) return;
    const qint64 videoId = item->data(Qt::UserRole).toLongLong();
    const Video video = m_database.findById(videoId);
    if (video.id == 0) return;
    m_tagManager->assignToVideo(videoId, video.fileName);
    showDetails(item);
}

void MainWindow::selectTag(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (!item) return;
    m_selectedTagId = item->data(0, Qt::UserRole).toLongLong();
    refreshVideos();
}

void MainWindow::refreshTags()
{
    if (!m_databaseReady || !m_tagManager) return;
    m_tagManager->refresh();
}

void MainWindow::showDetails(QListWidgetItem *item)
{
    if (!item) {
        m_selectedVideoPath.clear();
        m_details->setText(QStringLiteral("Select a video to view file info and tags."));
        for (QLabel *thumbnail : m_detailSampleLabels) {
            thumbnail->setText(QStringLiteral("Select a video"));
            thumbnail->setPixmap(QPixmap());
            thumbnail->setToolTip({});
        }
        return;
    }
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    Video video = m_database.findById(id);
    if (video.id != id) return;
    m_selectedVideoPath = video.filePath;

    // Probe metadata off the UI thread so slow files cannot block selection.
    if (video.durationMs <= 0 && !video.filePath.isEmpty() && QFileInfo::exists(video.filePath)) {
        if (!m_metadataProbeIds.contains(id)) {
            m_metadataProbeIds.insert(id);
            auto *watcher = new QFutureWatcher<VideoMetadataProbeResult>(this);
            connect(watcher, &QFutureWatcher<VideoMetadataProbeResult>::finished, this,
                    [this, watcher, id] {
                const VideoMetadataProbeResult result = watcher->result();
                m_metadataProbeIds.remove(id);
                watcher->deleteLater();

                if (!result.error.isEmpty()) {
                    if (result.video.filePath == m_selectedVideoPath) {
                        statusBar()->showMessage(result.error);
                    }
                    return;
                }

                QString error;
                if (!m_database.updateVideoMetadata(result.video, &error)) {
                    qWarning() << "Could not cache video metadata:" << error;
                    if (result.video.filePath == m_selectedVideoPath) {
                        statusBar()->showMessage(QStringLiteral("Could not save video metadata: %1").arg(error));
                    }
                    return;
                }

                if (result.video.durationMs > 0 &&
                    result.video.filePath == m_selectedVideoPath) {
                    showDetails(m_videoItemsByPath.value(result.video.filePath, nullptr));
                }
            });
            watcher->setFuture(QtConcurrent::run([video] {
                return probeVideoMetadata(video);
            }));
        }
    }

    const QString tags = m_database.tagsForVideo(id).join(QStringLiteral(", "));
    const double sizeGb = static_cast<double>(video.fileSize) / (1024.0 * 1024.0 * 1024.0);

    // 再生時間（ミリ秒）を HH:mm:ss または mm:ss にフォーマット
    auto formatDuration = [](qint64 ms) {
        if (ms <= 0) return QStringLiteral("Unknown");
        qint64 totalSecs = ms / 1000;
        qint64 hours = totalSecs / 3600;
        qint64 mins = (totalSecs % 3600) / 60;
        qint64 secs = totalSecs % 60;
        if (hours > 0) {
            return QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'));
        }
        return QStringLiteral("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    };

    const QString durationStr = formatDuration(video.durationMs);
    const QString resolutionStr = (video.width > 0 && video.height > 0)
        ? QStringLiteral("%1 × %2").arg(video.width).arg(video.height)
        : QStringLiteral("Unknown");
    const QString videoCodecStr = video.videoCodec.isEmpty() ? QStringLiteral("Unknown") : video.videoCodec;
    const QString audioCodecStr = video.audioCodec.isEmpty() ? QStringLiteral("Unknown") : video.audioCodec;
    const QString containerStr = video.containerFormat.isEmpty() ? QStringLiteral("Unknown") : video.containerFormat.toUpper();
    const QString favoriteStr = video.favorite ? QStringLiteral("Registered (★)") : QStringLiteral("Not Registered");

    m_details->setText(QStringLiteral(
        "<h2>%1</h2>"
        "<p><b>File Path:</b><br>%2</p>"
        "<hr>"
        "<p><b>Duration:</b> %3<br>"
        "<b>Resolution:</b> %4<br>"
        "<b>Format:</b> %5<br>"
        "<b>Video Codec:</b> %6<br>"
        "<b>Audio Codec:</b> %7<br>"
        "<b>File Size:</b> %8 GB<br>"
        "<b>Modified:</b> %9</p>"
        "<hr>"
        "<p><b>Favorite:</b> %10<br>"
        "<b>Play Count:</b> %11</p>"
        "<hr>"
        "<h3>Tags</h3><p>%12</p>")
        .arg(video.fileName.toHtmlEscaped(), video.filePath.toHtmlEscaped())
        .arg(durationStr)
        .arg(resolutionStr)
        .arg(containerStr.toHtmlEscaped())
        .arg(videoCodecStr.toHtmlEscaped())
        .arg(audioCodecStr.toHtmlEscaped())
        .arg(QString::number(sizeGb, 'f', 2))
        .arg(QDateTime::fromSecsSinceEpoch(video.fileMtime).toString(QStringLiteral("yyyy/MM/dd hh:mm")))
        .arg(favoriteStr)
        .arg(video.playCount)
        .arg(tags.isEmpty() ? QStringLiteral("Unknown") : tags.toHtmlEscaped()));

    for (int index = 0; index < m_detailSampleLabels.size(); ++index) {
        QLabel *thumbnail = m_detailSampleLabels.at(index);
        thumbnail->setPixmap(QPixmap());
        thumbnail->setToolTip({});
        if (video.durationMs <= 0) {
            thumbnail->setText(QStringLiteral("Duration unavailable"));
            continue;
        }
        const QString cachedPath = ThumbnailService::sampledCachedPath(video.filePath, index);
        const QPixmap cachedThumbnail(cachedPath);
        if (cachedThumbnail.isNull()) {
            thumbnail->setText(QStringLiteral("Generating..."));
        } else {
            thumbnail->setText({});
            thumbnail->setPixmap(cachedThumbnail);
            thumbnail->setToolTip(cachedPath);
        }
    }
    if (video.durationMs > 0) {
        m_thumbnailService.generateSamples(video.filePath, video.durationMs);
    }
}

void MainWindow::toggleFavorite()
{
    if (!m_databaseReady || !m_videoList->currentItem()) return;
    auto *item = m_videoList->currentItem();
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const Video video = m_database.findById(id);
    QString error;
    const bool newFavoriteState = !video.favorite;
    if (!m_database.setFavorite(id, newFavoriteState, &error)) {
        statusBar()->showMessage(QStringLiteral("Favorite update failed: %1").arg(error));
        return;
    }
    refreshVideos();
    statusBar()->showMessage(newFavoriteState ? QStringLiteral("Added to favorites (★)") : QStringLiteral("Removed from favorites"));
}

void MainWindow::playVideo(QListWidgetItem *item)
{
    if (!item) return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const Video video = m_database.findById(id);
    if (video.id == 0 || !QFileInfo::exists(video.filePath)) return;
    QString error;
    if (!m_database.incrementPlayCount(id, &error)) {
        statusBar()->showMessage(QStringLiteral("Play count update failed: %1").arg(error));
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(video.filePath))) {
        statusBar()->showMessage(QStringLiteral("Failed to launch external player: %1").arg(video.filePath));
    }
}

void MainWindow::showScanResult(int count, bool complete, const QStringList &errors)
{
    if (complete) {
        removeMissingVideosFromAvailableFolders();
    } else {
        qWarning() << "Media scan incomplete:" << errors.size() << "error(s)";
    }
    refreshVideos();
    refreshTags();
    if (!complete) {
        statusBar()->showMessage(QStringLiteral("Scan incomplete; missing-file cleanup was skipped. %1")
                                     .arg(errors.join(QStringLiteral("; "))));
        return;
    }
    if (count == 0) {
        statusBar()->showMessage(QStringLiteral("Scan complete: no supported video files found"));
    } else {
        statusBar()->showMessage(QStringLiteral("Scan complete: %1 videos discovered").arg(count));
    }
}

void MainWindow::removeMissingVideosFromAvailableFolders()
{
    struct RegisteredFolder
    {
        QString path;
        bool available;
    };

    QList<RegisteredFolder> registeredFolders;
    registeredFolders.reserve(m_folders.size());
    for (const QString &folder : m_folders) {
        const QString path = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
        const QDir directory(path);
        registeredFolders.append({path, directory.exists() && directory.isReadable()});
    }

    const QList<VideoLightItem> videos = m_database.searchVideosLight();
    for (const VideoLightItem &video : videos) {
        const QString filePath = QDir::cleanPath(QFileInfo(video.filePath).absoluteFilePath());
        int matchingFolder = -1;
        qsizetype longestMatch = -1;
        for (int i = 0; i < registeredFolders.size(); ++i) {
            const RegisteredFolder &folder = registeredFolders.at(i);
            const QString relativePath = QDir::fromNativeSeparators(QDir(folder.path).relativeFilePath(filePath));
            if (relativePath == QStringLiteral("..") || relativePath.startsWith(QStringLiteral("../"))) continue;
            if (folder.path.size() > longestMatch) {
                matchingFolder = i;
                longestMatch = folder.path.size();
            }
        }

        if (matchingFolder < 0 || !registeredFolders.at(matchingFolder).available ||
            QFileInfo(filePath).isFile()) {
            continue;
        }

        QString error;
        if (!m_database.deleteVideo(video.filePath, &error)) {
            qWarning() << "Could not remove missing video record:" << error;
            continue;
        }

        const QString thumbnailPath = ThumbnailService::cachedPath(video.filePath);
        if (QFileInfo::exists(thumbnailPath) && !QFile::remove(thumbnailPath)) {
            qWarning() << "Could not remove thumbnail for missing video";
        }
        m_thumbnailPaths.remove(video.filePath);
    }
}

#include <QStandardPaths>

void MainWindow::loadFolders()
{
    QSettings settings;
    const QString foldersKey = QStringLiteral("media/folders");
    const bool hasSavedFolderList = settings.contains(foldersKey);
    m_folders = settings.value(foldersKey).toStringList();

    const QString videosPath =
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    if (!hasSavedFolderList && m_folders.isEmpty() && !videosPath.isEmpty() &&
        QDir(videosPath).exists()) {
        m_folders.append(videosPath);
        saveFolders();
    }
    for (const QString &folder : m_folders)
        m_folderSelector->addItem(folder);

    if (!m_folders.isEmpty()) {
        m_currentFolder = m_folders.first();
        m_folderSelector->setCurrentIndex(0);
    }
    qInfo() << "Loaded media folder count:" << m_folders.size();
}

void MainWindow::saveFolders() const
{
    QSettings().setValue(QStringLiteral("media/folders"), m_folders);
}

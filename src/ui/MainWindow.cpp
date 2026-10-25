#include "ui/MainWindow.h"
#include "media/VideoMetadataProbe.h"

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
    loadFolders();
    refreshVideos();
    refreshTags();
    connect(&m_scanner, &MediaScanner::videosDiscovered, this,
            [this](const QList<Video> &videos) {
        if (!m_databaseReady || videos.isEmpty()) return;
        if (m_scanRegistrationCount == 0) m_scanRegistrationTimer.start();
        QString error;
        if (!m_database.upsertVideos(videos, &error)) {
            qWarning() << "Could not register scanned videos:" << error;
            statusBar()->showMessage(error);
            return;
        }
        m_scanRegistrationCount = videos.size();
        for (const Video &video : videos) {
            m_thumbnailService.generate(video.filePath);
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
        }
    });
    connect(&m_thumbnailService, &ThumbnailService::failed, this, [this](const QString &videoPath, const QString &message) {
        QListWidgetItem *item = m_videoItemsByPath.value(videoPath, nullptr);
        if (item) item->setIcon(QIcon());
        statusBar()->showMessage(message.isEmpty() ? QStringLiteral("Thumbnail generation failed") : QStringLiteral("Thumbnail failed: %1").arg(message));
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
    setWindowTitle(QStringLiteral("LightMediaBrowser v0.0.10-alpha"));
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
        QListWidget { background: #f5f7f8; border: 0; padding: 18px; outline: 0; }
        QListWidget::item { background: #ffffff; border: 1px solid #d7e0e3; border-radius: 8px; color: #1d2930; padding: 8px; }
        QListWidget::item:hover { background: #edf5f4; border-color: #2a9d8f; }
        QListWidget::item:selected { background: #d8eee9; border: 2px solid #2a9d8f; color: #1d2930; }
        QWidget#detailsPanel { background: #ffffff; border-left: 1px solid #d7e0e3; color: #33454d; }
        QLabel#sceneThumbnail { background: #edf1f2; border: 1px solid #d7e0e3; color: #52636a; }
        QScrollArea#detailsScroll { border: 0; }
        QStatusBar { background: #ffffff; color: #52636a; border-top: 1px solid #d7e0e3; }
    )");
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
    m_folderSelector->setMinimumWidth(220);
    toolbar->addWidget(m_folderSelector);
    
    toolbar->addSeparator();

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("🔍 Search videos..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(200);
    toolbar->addWidget(m_search);

    m_libraryFilter = new QComboBox(this);
    m_libraryFilter->addItem(QStringLiteral("All Videos"), false);
    m_libraryFilter->addItem(QStringLiteral("★ Favorites"), true);
    toolbar->addWidget(m_libraryFilter);

    m_duplicateFilter = new QComboBox(this);
    m_duplicateFilter->addItem(QStringLiteral("All"), false);
    m_duplicateFilter->addItem(QStringLiteral("Same-size candidates"), true);
    m_duplicateFilter->setToolTip(QStringLiteral(
        "Show files whose size matches another library file, even if the filename changed. "
        "Matching sizes do not confirm identical contents."));
    toolbar->addWidget(m_duplicateFilter);

    toolbar->addSeparator();

    m_sortSelector = new QComboBox(this);
    m_sortSelector->addItem(QStringLiteral("Newest"), QStringLiteral("newest"));
    m_sortSelector->addItem(QStringLiteral("Name"), QStringLiteral("name"));
    m_sortSelector->addItem(QStringLiteral("Size"), QStringLiteral("size"));
    m_sortSelector->addItem(QStringLiteral("Most Played"), QStringLiteral("plays"));
    toolbar->addWidget(m_sortSelector);

    m_minPlayCount = new QSpinBox(this);
    m_minPlayCount->setRange(0, 1000000);
    m_minPlayCount->setPrefix(QStringLiteral("Plays ≥ "));
    m_minPlayCount->setToolTip(QStringLiteral("Minimum play count"));
    toolbar->addWidget(m_minPlayCount);

    toolbar->addSeparator();

    auto *favoriteAction = toolbar->addAction(QStringLiteral("★ Favorite"));
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
    m_tagTree->setHeaderLabels({QStringLiteral("Tags")});
    m_tagTree->setMinimumWidth(190);
    m_tagTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_videoList = new QListWidget(splitter);
    m_videoList->setViewMode(QListView::IconMode);
    m_videoList->setResizeMode(QListView::Adjust);
    m_videoList->setMovement(QListView::Static);
    m_videoList->setUniformItemSizes(true);
    m_videoList->setIconSize(QSize(200, 112));
    m_videoList->setGridSize(QSize(230, 162));
    m_videoList->setSpacing(12);
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

    // 無限スクロール・遅延ロード対応：スクロールバーが一番下付近に達したら追加ロードする
    connect(m_videoList->verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this](int value) {
        auto *bar = m_videoList->verticalScrollBar();
        // スクロール位置が最大値の100px手前以内に達した場合に次のページをロード
        if (bar && value >= bar->maximum() - 100) {
            loadMoreVideos();
        }
    });

    statusBar()->showMessage(QStringLiteral("Ready"));
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
    qInfo() << "Starting scan across all registered folders:" << m_folders;
    statusBar()->showMessage(QStringLiteral("Scanning all folders..."));
    m_scanner.scanFolders(m_folders);
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

    const bool favoritesOnly = m_libraryFilter && m_libraryFilter->currentData().toBool();
    const bool duplicatesOnly = m_duplicateFilter && m_duplicateFilter->currentData().toBool();
    const QString query = m_search ? m_search->text() : QString();
    const int minPlayCount = m_minPlayCount ? m_minPlayCount->value() : 0;
    m_totalVideoCount = m_database.countVideosLight(query, m_selectedTagId, favoritesOnly,
                                                     minPlayCount, duplicatesOnly);
    loadMoreVideos();
}

void MainWindow::loadMoreVideos()
{
    if (!m_databaseReady || m_nextVideoOffset >= m_totalVideoCount) return;

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    const bool favoritesOnly = m_libraryFilter && m_libraryFilter->currentData().toBool();
    const bool duplicatesOnly = m_duplicateFilter && m_duplicateFilter->currentData().toBool();
    const int minPlayCount = m_minPlayCount ? m_minPlayCount->value() : 0;
    const QString query = m_search ? m_search->text() : QString();
    const QString sortKey = m_sortSelector ? m_sortSelector->currentData().toString() : QStringLiteral("name");
    const int targetCount = m_loadedVideoCount + m_pageSize;
    const QList<VideoLightItem> page = m_database.searchVideosLight(
        query, m_selectedTagId, favoritesOnly, minPlayCount, duplicatesOnly,
        sortKey, m_pageSize, m_nextVideoOffset);
    m_nextVideoOffset += page.size();
    const int loadedBeforePage = m_loadedVideoCount;
    for (const VideoLightItem &video : page) {
        const QFileInfo fileInfo(video.filePath);
        if (!fileInfo.exists() || !QDir(fileInfo.absolutePath()).exists()) continue;

        const QString marker = video.favorite ? QStringLiteral("★ ") : QString();
        auto *item = new QListWidgetItem(
            marker + video.fileName + QStringLiteral("\nPlays: ") + QString::number(video.playCount), m_videoList);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setData(Qt::UserRole, video.id);
        item->setToolTip(video.filePath);
        m_videoItemsByPath.insert(video.filePath, item);
        const QString thumbnailPath = m_thumbnailPaths.value(video.filePath, ThumbnailService::cachedPath(video.filePath));
        if (QFileInfo::exists(thumbnailPath)) item->setIcon(QIcon(thumbnailPath));
        ++m_loadedVideoCount;
    }
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
    qInfo() << "Video page UI update:" << page.size() << "rows queried;"
            << m_loadedVideoCount - loadedBeforePage << "rows displayed in"
            << elapsedTimer.elapsed() << "ms";
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
    QTreeWidgetItem *item = m_tagTree->itemAt(position);
    QMenu menu(this);

    if (!item) {
        // ツリーの余白を右クリック -> 大項目（カテゴリ）追加
        menu.addAction(QStringLiteral("＋ Add Category"), this, [this] {
            bool ok = false;
            QString catName = QInputDialog::getText(this, QStringLiteral("Add Category"), QStringLiteral("Category name:"), QLineEdit::Normal, QString(), &ok);
            if (ok && !catName.trimmed().isEmpty()) {
                QString error;
                if (!m_database.addCategory(catName.trimmed(), &error)) {
                    QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Could not add category: %1").arg(error));
                } else {
                    refreshTags();
                }
            }
        });
    } else if (item->parent() == nullptr) {
        // 大項目（カテゴリ）を右クリック -> 小項目（タグ）追加 / カテゴリ削除
        const QString catName = item->text(0);
        menu.addAction(QStringLiteral("＋ Add Tag to \"%1\"").arg(catName), this, [this, catName] {
            bool ok = false;
            QString tagName = QInputDialog::getText(this, QStringLiteral("Add Tag"), QStringLiteral("Tag name:"), QLineEdit::Normal, QString(), &ok);
            if (ok && !tagName.trimmed().isEmpty()) {
                QString error;
                if (!m_database.addTag(catName, tagName.trimmed(), &error)) {
                    QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Could not add tag: %1").arg(error));
                } else {
                    refreshTags();
                }
            }
        });
        menu.addSeparator();
        menu.addAction(QStringLiteral("🗑️ Delete Category \"%1\"").arg(catName), this, [this, catName] {
            if (QMessageBox::question(this, QStringLiteral("Delete Category"), QStringLiteral("Are you sure you want to delete category '%1' and all its tags?").arg(catName)) == QMessageBox::Yes) {
                QString error;
                if (!m_database.deleteCategory(catName, &error)) {
                    QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Could not delete category: %1").arg(error));
                } else {
                    m_selectedTagId = 0;
                    refreshTags();
                    refreshVideos();
                }
            }
        });
    } else {
        // 小項目（タグ）を右クリック -> タグ削除
        const qint64 tagId = item->data(0, Qt::UserRole).toLongLong();
        const QString tagName = item->text(0);
        menu.addAction(QStringLiteral("🗑️ Delete Tag \"%1\"").arg(tagName), this, [this, tagId, tagName] {
            if (QMessageBox::question(this, QStringLiteral("Delete Tag"), QStringLiteral("Are you sure you want to delete tag '%1'?").arg(tagName)) == QMessageBox::Yes) {
                QString error;
                if (!m_database.deleteTag(tagId, &error)) {
                    QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Could not delete tag: %1").arg(error));
                } else {
                    if (m_selectedTagId == tagId) m_selectedTagId = 0;
                    refreshTags();
                    refreshVideos();
                }
            }
        });
    }

    menu.exec(m_tagTree->viewport()->mapToGlobal(position));
}

void MainWindow::assignTagsToVideo()
{
    auto *item = m_videoList->currentItem();
    if (!item) return;
    const qint64 videoId = item->data(Qt::UserRole).toLongLong();
    const Video video = m_database.findById(videoId);
    if (video.id == 0) return;

    // タグ割り当てダイアログを作成
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Assign Tags: %1").arg(video.fileName));
    dialog.resize(360, 480);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("Select tags for this video:"), &dialog));

    // チェックボックス一覧を格納するスクロールエリア
    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    auto *scrollWidget = new QWidget();
    auto *scrollLayout = new QVBoxLayout(scrollWidget);

    const QList<TagInfo> allTags = m_database.tagsWithCategories();
    const QList<qint64> assignedTags = m_database.tagIdsForVideo(videoId);
    QMap<qint64, QCheckBox *> checkBoxMap;

    for (const TagInfo &tag : allTags) {
        if (tag.id <= 0) continue;
        auto *chk = new QCheckBox(QStringLiteral("[%1] %2").arg(tag.category, tag.name), scrollWidget);
        chk->setProperty("tagId", tag.id);
        if (assignedTags.contains(tag.id)) {
            chk->setChecked(true);
        }
        scrollLayout->addWidget(chk);
        checkBoxMap.insert(tag.id, chk);
    }
    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    layout->addWidget(scrollArea);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        QList<qint64> newAssignedTags;
        for (auto it = checkBoxMap.cbegin(); it != checkBoxMap.cend(); ++it) {
            if (it.value()->isChecked()) {
                newAssignedTags.append(it.key());
            }
        }
        QString error;
        if (!m_database.setVideoTags(videoId, newAssignedTags, &error)) {
            QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Could not assign tags: %1").arg(error));
        } else {
            showDetails(item);
            statusBar()->showMessage(QStringLiteral("Tags updated for: %1").arg(video.fileName));
        }
    }
}


void MainWindow::selectTag(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    m_selectedTagId = item->data(0, Qt::UserRole).toLongLong();
    refreshVideos();
}

void MainWindow::refreshTags()
{
    m_tagTree->clear();
    if (!m_databaseReady) return;

    QMap<QString, QTreeWidgetItem *> categories;

    for (const TagInfo &tag : m_database.tagsWithCategories()) {
        // カテゴリ項目の作成（未追加の場合のみ）
        if (!categories.contains(tag.category)) {
            categories.insert(tag.category, new QTreeWidgetItem(m_tagTree, {tag.category}));
        }

        // タグ名が存在する場合のみ、子要素（タグ）として追加
        if (!tag.name.isEmpty()) {
            auto *item = new QTreeWidgetItem(categories.value(tag.category), {tag.name});
            item->setData(0, Qt::UserRole, tag.id);
        }
    }

    for (QTreeWidgetItem *category : categories) {
        category->setExpanded(true);
    }
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
    if (m_scanRegistrationCount > 0) {
        qInfo() << "Video registration on UI thread:" << m_scanRegistrationCount
                << "videos in" << m_scanRegistrationTimer.elapsed() << "ms";
        m_scanRegistrationCount = 0;
    }
    if (complete) {
        removeMissingVideosFromAvailableFolders();
    } else {
        for (const QString &message : errors) qWarning() << "Media scan incomplete:" << message;
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
            qWarning() << "Could not remove missing video record:" << video.filePath << error;
            continue;
        }

        const QString thumbnailPath = ThumbnailService::cachedPath(video.filePath);
        if (QFileInfo::exists(thumbnailPath) && !QFile::remove(thumbnailPath)) {
            qWarning() << "Could not remove thumbnail for missing video:" << thumbnailPath;
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
    qInfo() << "Loaded media folders:" << m_folders;
}

void MainWindow::saveFolders() const
{
    QSettings().setValue(QStringLiteral("media/folders"), m_folders);
}

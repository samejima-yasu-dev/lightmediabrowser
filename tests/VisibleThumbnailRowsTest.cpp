#include "ui/VisibleThumbnailRows.h"
#include "ui/VideoListLayout.h"

#include <QListWidget>
#include <QMainWindow>
#include <QPainter>
#include <QSplitter>
#include <QStyleFactory>
#include <QToolBar>
#include <QTreeWidget>
#include <QtTest>

class VisibleThumbnailRowsTest : public QObject
{
    Q_OBJECT

private slots:
    void excludesRowsOutsideViewport();
    void gridHeightFitsThumbnailAndTwoTextLines();
    void displaysThumbnailBottomEdge();
    void itemGeometryRemainsValidWhenThumbnailArrivesAfterLayout();
};

void VisibleThumbnailRowsTest::excludesRowsOutsideViewport()
{
    const QList<QRect> itemRects = {
        QRect(0, -120, 100, 100),
        QRect(0, -20, 100, 100),
        QRect(0, 80, 100, 100),
        QRect(0, 180, 100, 100)
    };

    QCOMPARE(visibleThumbnailRows(itemRects, QRect(0, 0, 300, 180)),
             QList<int>({1, 2}));
}

void VisibleThumbnailRowsTest::gridHeightFitsThumbnailAndTwoTextLines()
{
    const QSize iconSize(200, 112);
    constexpr int lineHeight = 17;
    constexpr int verticalInsetsAndSpacing = 32;

    const QSize gridSize = videoListGridSize(iconSize, lineHeight);
    QVERIFY(gridSize.height() >= iconSize.height() + 2 * lineHeight + verticalInsetsAndSpacing);
}

void VisibleThumbnailRowsTest::displaysThumbnailBottomEdge()
{
    QListWidget list;
    list.setViewMode(QListView::IconMode);
    list.setResizeMode(QListView::Adjust);
    list.setMovement(QListView::Static);
    list.setUniformItemSizes(true);
    list.setIconSize(QSize(200, 112));
    list.setGridSize(videoListGridSize(list.iconSize(), list.fontMetrics().height()));
    list.setSpacing(12);
    list.setStyleSheet(QStringLiteral(
        "QListWidget { background: #121517; border: 0; padding: 18px; }"
        "QListWidget::item { background: #242a2d; border: 1px solid #343d3f; "
        "border-radius: 8px; padding: 8px; }"));
    list.setMinimumHeight(videoListMinimumHeight(list.gridSize()));
    list.resize(270, 120);

    QPixmap thumbnail(200, 112);
    thumbnail.fill(Qt::red);
    {
        QPainter painter(&thumbnail);
        painter.fillRect(QRect(0, 56, 200, 56), Qt::green);
    }
    auto *item = new QListWidgetItem(QStringLiteral("video.mp4\nPlays: 0"), &list);
    item->setIcon(QIcon(thumbnail));

    list.show();
    QTRY_VERIFY(list.visualItemRect(item).isValid());
    QImage rendered(list.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    list.render(&rendered);

    int greenPixelCount = 0;
    for (int y = 0; y < rendered.height(); ++y) {
        for (int x = 0; x < rendered.width(); ++x) {
            if (rendered.pixelColor(x, y) == QColor(Qt::green)) ++greenPixelCount;
        }
    }
    QVERIFY(greenPixelCount > 8000);
}

void VisibleThumbnailRowsTest::itemGeometryRemainsValidWhenThumbnailArrivesAfterLayout()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QMainWindow window;
    window.resize(2048, 1280);
    window.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #171a1d; color: #e7eceb; }
        QToolBar { padding: 9px 12px; }
        QSplitter::handle { background: #343b3f; }
        QListWidget { background: #121517; border: 0; padding: 18px; outline: 0; }
        QListWidget::item { background: #242a2d; border: 1px solid #343d3f;
                            border-radius: 8px; color: #e7eceb; padding: 8px; }
    )"));
    window.addToolBar(QStringLiteral("Library"));

    auto *splitter = new QSplitter(Qt::Horizontal, &window);
    auto *tagTree = new QTreeWidget(splitter);
    tagTree->setMinimumWidth(190);
    auto *list = new QListWidget(splitter);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setUniformItemSizes(true);
    list->setIconSize(QSize(200, 112));
    const QSize gridSize = videoListGridSize(list->iconSize(), list->fontMetrics().height());
    list->setGridSize(gridSize);
    list->setMinimumHeight(videoListMinimumHeight(gridSize));
    list->setSpacing(12);
    auto *details = new QWidget(splitter);
    details->setMinimumWidth(270);
    splitter->setSizes({190, 1514, 270});
    window.setCentralWidget(splitter);
    window.show();

    QPixmap thumbnail(200, 112);
    thumbnail.fill(Qt::green);
    QListWidgetItem *firstItem = nullptr;
    for (int index = 0; index < 2804; ++index) {
        auto *item = new QListWidgetItem(
            QStringLiteral("video-%1.mp4\nPlays: 0").arg(index), list);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setSizeHint(gridSize);
        if (index == 0) firstItem = item;
        if ((index + 1) % 100 == 0) QCoreApplication::processEvents();
    }

    QTRY_VERIFY(firstItem);
    QTRY_VERIFY(list->visualItemRect(firstItem).isValid());
    for (int index = 0; index < list->count(); ++index) {
        list->item(index)->setIcon(QIcon(thumbnail));
        if ((index + 1) % 100 == 0) QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    QCOMPARE(list->gridSize(), gridSize);
    const QRect itemRect = list->visualItemRect(firstItem);
    qInfo() << "Main-window geometry reproduction:"
            << "list:" << list->size()
            << "viewport:" << list->viewport()->size()
            << "grid:" << list->gridSize()
            << "item rect:" << itemRect
            << "item size hint:" << firstItem->sizeHint();
    QVERIFY2(itemRect.width() >= list->iconSize().width(),
             qPrintable(QStringLiteral("item rect: %1, grid: %2")
                            .arg(itemRect.width())
                            .arg(gridSize.width())));
    QVERIFY(itemRect.height() >= list->iconSize().height());

    QImage rendered(list->size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    list->render(&rendered);
    int greenPixelCount = 0;
    for (int y = 0; y < rendered.height(); ++y) {
        for (int x = 0; x < rendered.width(); ++x) {
            if (rendered.pixelColor(x, y) == QColor(Qt::green)) ++greenPixelCount;
        }
    }
    QVERIFY(greenPixelCount >= 200 * 112);
}

QTEST_MAIN(VisibleThumbnailRowsTest)
#include "VisibleThumbnailRowsTest.moc"

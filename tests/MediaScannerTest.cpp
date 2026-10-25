#include "scanner/MediaScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

class MediaScannerTest : public QObject
{
    Q_OBJECT

private slots:
    void scanEmitsAllVideosInOneBatchBeforeFinishing();
};

void MediaScannerTest::scanEmitsAllVideosInOneBatchBeforeFinishing()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString firstPath = temporaryDirectory.filePath(QStringLiteral("first.mp4"));
    const QString secondPath = temporaryDirectory.filePath(QStringLiteral("nested/second.mkv"));
    QVERIFY(QDir().mkpath(QFileInfo(secondPath).absolutePath()));
    for (const QString &path : {firstPath, secondPath,
                                temporaryDirectory.filePath(QStringLiteral("ignored.txt"))}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    MediaScanner scanner;
    int batchCount = 0;
    int finishedCount = -1;
    bool scanComplete = false;
    bool finished = false;
    QSet<QString> discoveredPaths;
    connect(&scanner, &MediaScanner::videosDiscovered, &scanner,
            [&](const QList<Video> &videos) {
        ++batchCount;
        for (const Video &video : videos) discoveredPaths.insert(video.filePath);
    });
    connect(&scanner, &MediaScanner::finished, &scanner,
            [&](int count, bool complete, const QStringList &) {
        finishedCount = count;
        scanComplete = complete;
        finished = true;
    });

    scanner.scan(temporaryDirectory.path());
    QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

    QCOMPARE(batchCount, 1);
    QCOMPARE(finishedCount, 2);
    QVERIFY(scanComplete);
    QCOMPARE(discoveredPaths, QSet<QString>({firstPath, secondPath}));
}

QTEST_GUILESS_MAIN(MediaScannerTest)
#include "MediaScannerTest.moc"

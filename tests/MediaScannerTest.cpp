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
    void missingRootLeavesScanIncomplete();
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

void MediaScannerTest::missingRootLeavesScanIncomplete()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missingRoot = temporaryDirectory.filePath(QStringLiteral("missing-folder"));

    MediaScanner scanner;
    bool finished = false;
    bool complete = true;
    int discoveredCount = -1;
    QStringList errors;
    connect(&scanner, &MediaScanner::videosDiscovered, &scanner,
            [&](const QList<Video> &videos) { discoveredCount = videos.size(); });
    connect(&scanner, &MediaScanner::finished, &scanner,
            [&](int, bool scanComplete, const QStringList &scanErrors) {
        finished = true;
        complete = scanComplete;
        errors = scanErrors;
    });

    scanner.scan(missingRoot);
    QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

    QVERIFY(!complete);
    QVERIFY(errors.size() >= 1);
    QCOMPARE(discoveredCount, 0);
}

QTEST_GUILESS_MAIN(MediaScannerTest)
#include "MediaScannerTest.moc"

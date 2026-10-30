#include "media/ThumbnailService.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>

class ThumbnailServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanupTestCase();
    void samplesCoverVideoWithoutUsingFirstFrame();
    void invalidDurationProducesNoSamples();
    void sampleCachesDoNotReplaceListThumbnail();
    void generateSamplesEmitsThreeCachedImages();
    void generateAllCreatesMissingThumbnail();
    void generateAllReportsMissingVideoAsFailure();
    void generateAllSkipsExistingThumbnailsAndReportsCompletion();
    void cancelGenerateAllStopsDequeuingPendingVideos();

private:
    QStringList m_generatedCachePaths;
};

void ThumbnailServiceTest::samplesCoverVideoWithoutUsingFirstFrame()
{
    QCOMPARE(ThumbnailService::sampleTimestamps(100000),
             QList<qint64>({5000, 50000, 95000}));
}

void ThumbnailServiceTest::invalidDurationProducesNoSamples()
{
    QVERIFY(ThumbnailService::sampleTimestamps(0).isEmpty());
    QVERIFY(ThumbnailService::sampleTimestamps(-1).isEmpty());
}

void ThumbnailServiceTest::sampleCachesDoNotReplaceListThumbnail()
{
    const QString videoPath = QStringLiteral("/videos/example.mp4");
    const QString listThumbnail = ThumbnailService::cachedPath(videoPath);
    QStringList samplePaths;
    for (int index = 0; index < 3; ++index) {
        const QString path = ThumbnailService::sampledCachedPath(videoPath, index);
        QVERIFY(!path.isEmpty());
        QVERIFY(path != listThumbnail);
        QVERIFY(!samplePaths.contains(path));
        samplePaths.append(path);
    }
}

void ThumbnailServiceTest::generateSamplesEmitsThreeCachedImages()
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate sample thumbnails");

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("sample.mp4"));
    QProcess creator;
    creator.start(ffmpeg, {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("testsrc2=size=320x180:rate=5:duration=10"),
        QStringLiteral("-an"),
        QStringLiteral("-y"),
        videoPath
    });
    QVERIFY(creator.waitForStarted());
    QVERIFY(creator.waitForFinished(30000));
    QCOMPARE(creator.exitStatus(), QProcess::NormalExit);
    QCOMPARE(creator.exitCode(), 0);

    ThumbnailService service;
    QSignalSpy generated(&service, &ThumbnailService::sampleGenerated);
    QSignalSpy failed(&service, &ThumbnailService::sampleFailed);
    QVERIFY(generated.isValid());
    QVERIFY(failed.isValid());
    service.generateSamples(videoPath, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(generated.count(), 3, 30000);

    QSet<int> sampleIndices;
    for (const QList<QVariant> &arguments : generated) {
        QCOMPARE(arguments.at(0).toString(), videoPath);
        const int index = arguments.at(1).toInt();
        QVERIFY(index >= 0 && index < 3);
        sampleIndices.insert(index);
        const QString path = arguments.at(2).toString();
        QVERIFY(QFileInfo::exists(path));
        m_generatedCachePaths.append(path);
    }
    QCOMPARE(sampleIndices.size(), 3);

    QVERIFY(QFile::remove(videoPath));
    service.generateSamples(videoPath, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(generated.count() + failed.count() == 6, 30000);
    QCOMPARE(generated.count(), 6);
    QVERIFY(failed.isEmpty());
}

void ThumbnailServiceTest::generateAllSkipsExistingThumbnailsAndReportsCompletion()
{
    const QString firstVideo = QStringLiteral("/videos/bulk-first.mp4");
    const QString secondVideo = QStringLiteral("/videos/bulk-second.mp4");
    const QString firstCache = ThumbnailService::cachedPath(firstVideo);
    const QString secondCache = ThumbnailService::cachedPath(secondVideo);
    QVERIFY(QDir().mkpath(QFileInfo(firstCache).absolutePath()));
    QImage thumbnail(320, 180, QImage::Format_RGB32);
    thumbnail.fill(Qt::blue);
    QVERIFY(thumbnail.save(firstCache, "JPG"));
    QVERIFY(thumbnail.save(secondCache, "JPG"));
    m_generatedCachePaths << firstCache << secondCache;

    ThumbnailService service;
    QSignalSpy progress(&service, &ThumbnailService::bulkProgress);
    QSignalSpy finished(&service, &ThumbnailService::bulkFinished);
    QVERIFY(progress.isValid());
    QVERIFY(finished.isValid());

    QVERIFY(service.generateAll({firstVideo, secondVideo, firstVideo}));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);

    const QList<QVariant> result = finished.takeFirst();
    QCOMPARE(result.at(0).toInt(), 2);
    QCOMPARE(result.at(1).toInt(), 0);
    QCOMPARE(result.at(2).toInt(), 2);
    QCOMPARE(result.at(3).toInt(), 0);
    QCOMPARE(result.at(4).toBool(), false);
    QVERIFY(!progress.isEmpty());
    QCOMPARE(progress.last().at(0).toInt(), 2);
    QCOMPARE(progress.last().at(1).toInt(), 2);
    QCOMPARE(progress.last().at(2).toInt(), 0);
    QCOMPARE(progress.last().at(3).toInt(), 2);
    QCOMPARE(progress.last().at(4).toInt(), 0);
}

void ThumbnailServiceTest::generateAllCreatesMissingThumbnail()
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to generate a thumbnail");

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("bulk.mp4"));
    QProcess creator;
    creator.start(ffmpeg, {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("testsrc2=size=320x180:rate=5:duration=1"),
        QStringLiteral("-an"),
        QStringLiteral("-y"),
        videoPath
    });
    QVERIFY(creator.waitForStarted());
    QVERIFY(creator.waitForFinished(30000));
    QCOMPARE(creator.exitStatus(), QProcess::NormalExit);
    QCOMPARE(creator.exitCode(), 0);

    ThumbnailService service;
    QSignalSpy finished(&service, &ThumbnailService::bulkFinished);
    QVERIFY(finished.isValid());
    QVERIFY(service.generateAll({videoPath}));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);

    const QList<QVariant> result = finished.takeFirst();
    QCOMPARE(result.at(0).toInt(), 1);
    QCOMPARE(result.at(1).toInt(), 1);
    QCOMPARE(result.at(2).toInt(), 0);
    QCOMPARE(result.at(3).toInt(), 0);
    QCOMPARE(result.at(4).toBool(), false);
    const QString thumbnailPath = ThumbnailService::cachedPath(videoPath);
    QVERIFY(QFileInfo::exists(thumbnailPath));
    m_generatedCachePaths.append(thumbnailPath);
}

void ThumbnailServiceTest::generateAllReportsMissingVideoAsFailure()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missingVideo = temporaryDirectory.filePath(QStringLiteral("missing.mp4"));

    ThumbnailService service;
    QSignalSpy finished(&service, &ThumbnailService::bulkFinished);
    QVERIFY(finished.isValid());
    QVERIFY(service.generateAll({missingVideo}));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);

    const QList<QVariant> result = finished.takeFirst();
    QCOMPARE(result.at(0).toInt(), 1);
    QCOMPARE(result.at(1).toInt(), 0);
    QCOMPARE(result.at(2).toInt(), 0);
    QCOMPARE(result.at(3).toInt(), 1);
    QCOMPARE(result.at(4).toBool(), false);
}

void ThumbnailServiceTest::cancelGenerateAllStopsDequeuingPendingVideos()
{
    QStringList videoPaths;
    for (int index = 0; index < 10000; ++index) {
        videoPaths.append(QStringLiteral("/missing/bulk-%1.mp4").arg(index));
    }

    ThumbnailService service;
    QSignalSpy finished(&service, &ThumbnailService::bulkFinished);
    QVERIFY(finished.isValid());
    QVERIFY(service.generateAll(videoPaths));
    service.cancelGenerateAll();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    const QList<QVariant> result = finished.takeFirst();
    QCOMPARE(result.at(0).toInt(), 10000);
    QVERIFY(result.at(4).toBool());
    QVERIFY(result.at(1).toInt() + result.at(2).toInt() + result.at(3).toInt() < 10000);
}

void ThumbnailServiceTest::cleanupTestCase()
{
    for (const QString &path : m_generatedCachePaths) {
        QFile::remove(path);
    }
}

QTEST_APPLESS_MAIN(ThumbnailServiceTest)
#include "ThumbnailServiceTest.moc"

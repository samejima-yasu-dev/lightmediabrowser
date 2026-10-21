#include "media/ThumbnailService.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
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

void ThumbnailServiceTest::cleanupTestCase()
{
    for (const QString &path : m_generatedCachePaths) {
        QFile::remove(path);
    }
}

QTEST_APPLESS_MAIN(ThumbnailServiceTest)
#include "ThumbnailServiceTest.moc"

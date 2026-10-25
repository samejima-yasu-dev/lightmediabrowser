#include "media/VideoMetadataProbe.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class VideoMetadataProbeTest : public QObject
{
    Q_OBJECT

private slots:
    void readsMetadataFromVideoFile();
};

void VideoMetadataProbeTest::readsMetadataFromVideoFile()
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) QSKIP("ffmpeg is required to create a test video");
    if (QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty()) {
        QSKIP("ffprobe is required to read video metadata");
    }

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("metadata.mp4"));
    QProcess creator;
    creator.start(ffmpeg, {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("testsrc2=size=320x180:rate=5:duration=2"),
        QStringLiteral("-an"),
        QStringLiteral("-c:v"), QStringLiteral("mpeg4"),
        QStringLiteral("-y"),
        videoPath
    });
    QVERIFY(creator.waitForStarted());
    QVERIFY(creator.waitForFinished(30000));
    QCOMPARE(creator.exitStatus(), QProcess::NormalExit);
    QCOMPARE(creator.exitCode(), 0);
    QVERIFY(QFileInfo::exists(videoPath));

    Video video;
    video.filePath = videoPath;
    video.fileName = QStringLiteral("metadata.mp4");
    const VideoMetadataProbeResult result = probeVideoMetadata(video);

    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.video.filePath, videoPath);
    QCOMPARE(result.video.width, 320);
    QCOMPARE(result.video.height, 180);
    QVERIFY(result.video.durationMs >= 1000);
    QVERIFY(!result.video.videoCodec.isEmpty());
}

QTEST_GUILESS_MAIN(VideoMetadataProbeTest)
#include "VideoMetadataProbeTest.moc"

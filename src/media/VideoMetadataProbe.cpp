#include "media/VideoMetadataProbe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>

VideoMetadataProbeResult probeVideoMetadata(const Video &video)
{
    VideoMetadataProbeResult result;
    result.video = video;

    QProcess probe;
    probe.start(QStringLiteral("ffprobe"), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"), QStringLiteral("-show_streams"),
        video.filePath
    });
    if (!probe.waitForStarted(1000)) {
        result.error = QStringLiteral("Could not start ffprobe: %1").arg(probe.errorString());
        return result;
    }
    if (!probe.waitForFinished(5000)) {
        probe.kill();
        probe.waitForFinished();
        result.error = QStringLiteral("ffprobe timed out for: %1").arg(video.fileName);
        return result;
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        result.error = QString::fromLocal8Bit(probe.readAllStandardError()).trimmed();
        if (result.error.isEmpty()) {
            result.error = QStringLiteral("ffprobe failed for: %1").arg(video.fileName);
        }
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(probe.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("Could not parse ffprobe output: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = document.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    const double duration = format.value(QStringLiteral("duration")).toString().toDouble();
    if (duration > 0) result.video.durationMs = static_cast<qint64>(duration * 1000.0);
    result.video.containerFormat = format.value(QStringLiteral("format_name")).toString()
                                       .section(QLatin1Char(','), 0, 0).toUpper();
    for (const QJsonValue &streamValue : root.value(QStringLiteral("streams")).toArray()) {
        const QJsonObject stream = streamValue.toObject();
        const QString codec = stream.value(QStringLiteral("codec_name")).toString().toUpper();
        if (stream.value(QStringLiteral("codec_type")).toString() == QStringLiteral("video") &&
            result.video.videoCodec.isEmpty()) {
            result.video.videoCodec = codec;
            result.video.width = stream.value(QStringLiteral("width")).toInt();
            result.video.height = stream.value(QStringLiteral("height")).toInt();
        } else if (stream.value(QStringLiteral("codec_type")).toString() == QStringLiteral("audio") &&
                   result.video.audioCodec.isEmpty()) {
            result.video.audioCodec = codec;
        }
    }

    return result;
}

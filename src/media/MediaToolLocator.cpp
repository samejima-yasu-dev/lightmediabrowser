#include "media/MediaToolLocator.h"

#include <QStandardPaths>
#include <QStringList>

MediaToolAvailability locateMediaTools()
{
    MediaToolAvailability result;
#ifdef Q_OS_WIN
    result.ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg.exe"));
    result.ffprobePath = QStandardPaths::findExecutable(QStringLiteral("ffprobe.exe"));
#else
    result.ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    result.ffprobePath = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
#endif
    result.ffmpegAvailable = !result.ffmpegPath.isEmpty();
    result.ffprobeAvailable = !result.ffprobePath.isEmpty();
    return result;
}

QString missingMediaToolsMessage(const MediaToolAvailability &availability)
{
    QStringList missing;
    if (!availability.ffmpegAvailable) missing.append(QStringLiteral("ffmpeg"));
    if (!availability.ffprobeAvailable) missing.append(QStringLiteral("ffprobe"));
    if (missing.isEmpty()) return {};

    QString hint;
#ifdef Q_OS_WIN
    hint = QStringLiteral("Add the folder containing ffmpeg.exe and ffprobe.exe to PATH.");
#elif defined(Q_OS_MAC)
    hint = QStringLiteral("Install with \"brew install ffmpeg\" and ensure the tools are on PATH.");
#else
    hint = QStringLiteral("Install FFmpeg with your system package manager.");
#endif
    return QStringLiteral("Missing %1. %2").arg(missing.join(QStringLiteral(" and ")), hint);
}

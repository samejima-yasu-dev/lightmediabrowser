#include "media/MediaToolLocator.h"

#include <QCoreApplication>
#include <QtTest>

bool missingMediaToolsMessageIsActionable()
{
    MediaToolAvailability availability;
    availability.ffmpegAvailable = false;
    availability.ffprobeAvailable = false;

    const QString message = missingMediaToolsMessage(availability);
    return message.contains(QStringLiteral("ffmpeg")) &&
           message.contains(QStringLiteral("ffprobe"));
}

bool presentToolsProduceNoErrorMessage()
{
    MediaToolAvailability availability;
    availability.ffmpegAvailable = true;
    availability.ffprobeAvailable = true;
    return missingMediaToolsMessage(availability).isEmpty();
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (!missingMediaToolsMessageIsActionable()) return 1;
    return presentToolsProduceNoErrorMessage() ? 0 : 1;
}

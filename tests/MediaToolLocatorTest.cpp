#include "media/MediaToolLocator.h"

#include <QCoreApplication>
#include <QtTest>

void missingMediaToolsMessageIsActionable()
{
    MediaToolAvailability availability;
    availability.ffmpegAvailable = false;
    availability.ffprobeAvailable = false;

    const QString message = missingMediaToolsMessage(availability);
    QVERIFY(message.contains(QStringLiteral("ffmpeg")));
    QVERIFY(message.contains(QStringLiteral("ffprobe")));
}

void presentToolsProduceNoErrorMessage()
{
    MediaToolAvailability availability;
    availability.ffmpegAvailable = true;
    availability.ffprobeAvailable = true;
    QVERIFY(missingMediaToolsMessage(availability).isEmpty());
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    missingMediaToolsMessageIsActionable();
    presentToolsProduceNoErrorMessage();
    return 0;
}

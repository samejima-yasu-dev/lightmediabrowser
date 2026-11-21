#pragma once

#include <QString>

struct MediaToolAvailability
{
    bool ffmpegAvailable = false;
    bool ffprobeAvailable = false;
    QString ffmpegPath;
    QString ffprobePath;
};

MediaToolAvailability locateMediaTools();
QString missingMediaToolsMessage(const MediaToolAvailability &availability);

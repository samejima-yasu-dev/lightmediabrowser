#pragma once

#include "domain/Video.h"

#include <QString>

struct VideoMetadataProbeResult
{
    Video video;
    QString error;
};

VideoMetadataProbeResult probeVideoMetadata(const Video &video);

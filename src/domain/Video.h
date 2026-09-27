#pragma once

#include <QString>
#include <QDateTime>

struct Video
{
    qint64 id = 0;
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    qint64 fileMtime = 0;
    qint64 durationMs = 0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
    QString containerFormat;
    int rating = 0;
    QString note;
    bool favorite = false;
    int playCount = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#include "scanner/MediaScanner.h"

#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QSet>
#include <QStringList>

MediaScanner::MediaScanner(QObject *parent) : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<QList<Video>>::finished, this, [this] {
        const QList<Video> videos = m_watcher.result();
        qInfo() << "Media scan finished:" << videos.size();
        for (const Video &video : videos) emit videoDiscovered(video);
        emit finished(videos.size());
    });
}

void MediaScanner::scan(const QString &folder)
{
    scanFolders(QStringList{folder});
}

void MediaScanner::scanFolders(const QStringList &folders)
{
    if (m_watcher.isRunning()) return;
    MediaScanner *scanner = this;
    m_watcher.setFuture(QtConcurrent::run([folders, scanner] {
        QList<Video> videos;
        const QSet<QString> extensions = {
            QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), 
            QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("m4v"), 
            QStringLiteral("ts"), QStringLiteral("mpeg"), QStringLiteral("mpg"),
            QStringLiteral("wmv"), QStringLiteral("flv"), QStringLiteral("m2ts")
        };
        for (const QString &folder : folders) {
            if (folder.isEmpty()) continue;
            QDirIterator iterator(folder, QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
            while (iterator.hasNext()) {
                const QFileInfo info(iterator.next());
                if (!extensions.contains(info.suffix().toLower())) continue;
                Video video;
                video.filePath = info.absoluteFilePath();
                video.fileName = info.fileName();
                video.fileSize = info.size();
                video.fileMtime = info.lastModified().toSecsSinceEpoch();
                videos.append(video);
                const int count = videos.size();
                QMetaObject::invokeMethod(scanner, [scanner, count] { emit scanner->progress(count); }, Qt::QueuedConnection);
            }
            qInfo() << "Media scan found" << videos.size() << "videos so far after folder" << folder;
        }
        qInfo() << "Total media scan found" << videos.size() << "videos across all folders";
        return videos;
    }));
}

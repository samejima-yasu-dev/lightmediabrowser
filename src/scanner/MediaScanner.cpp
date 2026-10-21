#include "scanner/MediaScanner.h"

#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QSet>
#include <QStringList>

MediaScanner::MediaScanner(QObject *parent) : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<ScanResult>::finished, this, [this] {
        const ScanResult result = m_watcher.result();
        qInfo() << "Media scan finished:" << result.videos.size()
                << "videos in" << result.elapsedMs << "ms; complete:"
                << result.complete << "errors:" << result.errors;
        for (const Video &video : result.videos) emit videoDiscovered(video);
        emit finished(result.videos.size(), result.complete, result.errors);
    });
}

MediaScanner::~MediaScanner()
{
    m_watcher.waitForFinished();
}

void MediaScanner::scan(const QString &folder)
{
    scanFolders(QStringList{folder});
}

void MediaScanner::scanFolders(const QStringList &folders)
{
    if (m_watcher.isRunning()) {
        emit finished(0, false, {QStringLiteral("A scan is already in progress")});
        return;
    }
    MediaScanner *scanner = this;
    m_watcher.setFuture(QtConcurrent::run([folders, scanner] {
        ScanResult result;
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        const QSet<QString> extensions = {
            QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
            QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("m4v"),
            QStringLiteral("ts"), QStringLiteral("mpeg"), QStringLiteral("mpg"),
            QStringLiteral("wmv"), QStringLiteral("flv"), QStringLiteral("m2ts")
        };
        QSet<QString> visitedDirectories;
        QSet<QString> discoveredPaths;

        for (const QString &folder : folders) {
            if (folder.isEmpty()) continue;
            visitedDirectories.clear();

            const QFileInfo rootInfo(folder);
            if (!rootInfo.exists() || !rootInfo.isDir() || !rootInfo.isReadable()) {
                result.complete = false;
                result.errors.append(QStringLiteral("Folder is unavailable or unreadable: %1").arg(folder));
                continue;
            }

            QStringList pendingDirectories{rootInfo.absoluteFilePath()};
            while (!pendingDirectories.isEmpty()) {
                const QString path = pendingDirectories.takeLast();
                const QFileInfo directoryInfo(path);
                const QString canonicalPath = directoryInfo.canonicalFilePath();
                const QString directoryKey = canonicalPath.isEmpty()
                    ? QDir::cleanPath(directoryInfo.absoluteFilePath())
                    : canonicalPath;
                if (visitedDirectories.contains(directoryKey)) continue;
                visitedDirectories.insert(directoryKey);

                if (!directoryInfo.exists() || !directoryInfo.isDir() || !directoryInfo.isReadable()) {
                    result.complete = false;
                    result.errors.append(QStringLiteral("Directory is unreadable: %1").arg(path));
                    continue;
                }

                const QFileInfoList entries = QDir(path).entryInfoList(
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
                for (const QFileInfo &info : entries) {
                    if (info.isDir()) {
                        pendingDirectories.append(info.absoluteFilePath());
                        continue;
                    }
                    if (!info.isFile() || !extensions.contains(info.suffix().toLower())) continue;
                    const QString filePath = QDir::cleanPath(info.absoluteFilePath());
                    if (discoveredPaths.contains(filePath)) continue;
                    discoveredPaths.insert(filePath);

                    Video video;
                    video.filePath = filePath;
                    video.fileName = info.fileName();
                    video.fileSize = info.size();
                    video.fileMtime = info.lastModified().toSecsSinceEpoch();
                    result.videos.append(video);
                    const int count = result.videos.size();
                    if (count % 100 == 0) {
                        QMetaObject::invokeMethod(scanner, [scanner, count] { emit scanner->progress(count); }, Qt::QueuedConnection);
                    }
                }
            }
            qInfo() << "Media scan found" << result.videos.size() << "videos so far after folder" << folder;
        }
        qInfo() << "Total media scan found" << result.videos.size() << "videos across all folders";
        result.elapsedMs = elapsedTimer.elapsed();
        return result;
    }));
}

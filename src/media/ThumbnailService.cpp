#include "media/ThumbnailService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <limits>

namespace
{
bool isValidThumbnail(const QString &path)
{
    QImageReader reader(path);
    return reader.size() == QSize(320, 180) && !reader.read().isNull();
}
}

ThumbnailService::ThumbnailService(QObject *parent) : QObject(parent)
{
    setThreadLimitEnabled(true);
}

void ThumbnailService::setThreadLimitEnabled(bool enabled)
{
    // 制限有効時はCPU負荷を抑えるため、並列生成スレッド数を最大4に抑制
    const int threadCount = enabled
        ? qBound(1, QThread::idealThreadCount() / 2, 4)
        : std::numeric_limits<int>::max();
    m_pool.setMaxThreadCount(threadCount);
}

ThumbnailService::~ThumbnailService()
{
    m_pool.waitForDone();
}

QString ThumbnailService::cachedPath(const QString &videoPath)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/thumbnails");
    const QByteArray key = QCryptographicHash::hash(videoPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return cacheDir + QLatin1Char('/') + QString::fromLatin1(key) + QStringLiteral("-320x180-v2.jpg");
}

void ThumbnailService::generate(const QString &videoPath)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_generating.contains(videoPath)) return;
        m_generating.insert(videoPath);
    }

    m_pool.start([this, videoPath] {
        const auto finish = [this, &videoPath] {
            QMutexLocker locker(&m_mutex);
            m_generating.remove(videoPath);
        };

        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/thumbnails");
        if (!QDir().mkpath(cacheDir)) {
            finish();
            emit failed(videoPath, QStringLiteral("Failed to create thumbnail cache"));
            return;
        }
        const QString thumbnailPath = cachedPath(videoPath);
        if (isValidThumbnail(thumbnailPath)) {
            finish();
            return;
        }

        const QString temporaryPath = thumbnailPath + QLatin1Char('.')
            + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".jpg");
        QProcess process;
        process.setProgram(QStringLiteral("ffmpeg"));
        auto runAt = [&](const QString &timestamp) {
            process.setArguments({
                QStringLiteral("-hide_banner"),
                QStringLiteral("-loglevel"), QStringLiteral("error"),
                QStringLiteral("-threads"), QStringLiteral("2"),
                QStringLiteral("-y"),
                QStringLiteral("-ss"), timestamp,
                QStringLiteral("-i"), videoPath,
                QStringLiteral("-frames:v"), QStringLiteral("1"),
                QStringLiteral("-vf"),
                QStringLiteral("scale=320:180:force_original_aspect_ratio=decrease,"
                               "pad=320:180:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1"),
                temporaryPath
            });
            process.start();
            const bool finished = process.waitForFinished(30000);
            if (!finished && process.state() != QProcess::NotRunning) {
                process.kill();
                process.waitForFinished();
            }
            return finished && process.exitStatus() == QProcess::NormalExit
                && process.exitCode() == 0 && isValidThumbnail(temporaryPath);
        };
        bool succeeded = runAt(QStringLiteral("00:02:00"));
        if (!succeeded) {
            QFile::remove(temporaryPath);
            succeeded = runAt(QStringLiteral("00:00:00"));
        }
        if (succeeded) {
            QFile source(temporaryPath);
            QSaveFile destination(thumbnailPath);
            destination.setDirectWriteFallback(false);
            succeeded = source.open(QIODevice::ReadOnly) && destination.open(QIODevice::WriteOnly);
            if (succeeded) {
                const QByteArray contents = source.readAll();
                succeeded = !contents.isEmpty()
                    && destination.write(contents) == contents.size()
                    && destination.commit();
            }
        }
        QFile::remove(temporaryPath);
        if (!succeeded) QFile::remove(thumbnailPath);
        finish();
        if (!succeeded) {
            QString message = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (message.isEmpty()) message = process.errorString();
            emit failed(videoPath, message);
            return;
        }
        emit generated(videoPath, thumbnailPath);
    });
}

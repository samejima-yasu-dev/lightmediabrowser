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
constexpr int SampleCount = 3;

bool isValidThumbnail(const QString &path)
{
    QImageReader reader(path);
    return reader.size() == QSize(320, 180) && !reader.read().isNull();
}

QString thumbnailCacheDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/thumbnails");
}

bool createThumbnail(const QString &videoPath, const QList<qint64> &timestampsMs,
                     const QString &thumbnailPath, QString *error)
{
    if (isValidThumbnail(thumbnailPath)) return true;

    const QString temporaryPath = thumbnailPath + QLatin1Char('.')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".jpg");
    QString processError;
    bool succeeded = false;
    for (const qint64 timestampMs : timestampsMs) {
        QProcess process;
        process.setProgram(QStringLiteral("ffmpeg"));
        process.setArguments({
            QStringLiteral("-hide_banner"),
            QStringLiteral("-loglevel"), QStringLiteral("error"),
            QStringLiteral("-threads"), QStringLiteral("2"),
            QStringLiteral("-y"),
            QStringLiteral("-ss"), QString::number(timestampMs / 1000.0, 'f', 3),
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
        succeeded = finished && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0 && isValidThumbnail(temporaryPath);
        if (succeeded) break;

        processError = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (processError.isEmpty()) processError = process.errorString();
        QFile::remove(temporaryPath);
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
    if (!succeeded) {
        QFile::remove(thumbnailPath);
        if (error) {
            *error = processError.isEmpty()
                ? QStringLiteral("Failed to save generated thumbnail")
                : processError;
        }
    }
    return succeeded;
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
    const QByteArray key = QCryptographicHash::hash(videoPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return thumbnailCacheDirectory() + QLatin1Char('/') + QString::fromLatin1(key)
        + QStringLiteral("-320x180-v2.jpg");
}

QList<qint64> ThumbnailService::sampleTimestamps(qint64 durationMs)
{
    if (durationMs <= 0) return {};

    QList<qint64> timestamps;
    timestamps.reserve(SampleCount);
    for (int index = 0; index < SampleCount; ++index) {
        const double fraction = 0.05 + 0.9 * index / (SampleCount - 1);
        timestamps.append(qRound64(durationMs * fraction));
    }
    return timestamps;
}

QString ThumbnailService::sampledCachedPath(const QString &videoPath, int sampleIndex)
{
    if (sampleIndex < 0 || sampleIndex >= SampleCount) return {};
    const QByteArray key = QCryptographicHash::hash(videoPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return thumbnailCacheDirectory() + QLatin1Char('/') + QString::fromLatin1(key)
        + QStringLiteral("-sample-%1-320x180-v1.jpg").arg(sampleIndex);
}

void ThumbnailService::generate(const QString &videoPath)
{
    const QString thumbnailPath = cachedPath(videoPath);
    {
        QMutexLocker locker(&m_mutex);
        if (m_generating.contains(thumbnailPath)) return;
        m_generating.insert(thumbnailPath);
    }

    m_pool.start([this, videoPath, thumbnailPath] {
        const auto finish = [this, &videoPath] {
            QMutexLocker locker(&m_mutex);
            m_generating.remove(cachedPath(videoPath));
        };

        if (!QDir().mkpath(thumbnailCacheDirectory())) {
            finish();
            emit failed(videoPath, QStringLiteral("Failed to create thumbnail cache"));
            return;
        }
        if (isValidThumbnail(thumbnailPath)) {
            finish();
            return;
        }
        QString error;
        const bool succeeded = createThumbnail(videoPath, {120000, 0}, thumbnailPath, &error);
        finish();
        if (!succeeded) {
            emit failed(videoPath, error);
            return;
        }
        emit generated(videoPath, thumbnailPath);
    });
}

void ThumbnailService::generateSamples(const QString &videoPath, qint64 durationMs)
{
    struct SampleTask
    {
        int index;
        qint64 timestampMs;
        QString thumbnailPath;
    };

    const QList<qint64> timestamps = sampleTimestamps(durationMs);
    QList<SampleTask> tasks;
    for (int sampleIndex = 0; sampleIndex < timestamps.size(); ++sampleIndex) {
        const QString thumbnailPath = sampledCachedPath(videoPath, sampleIndex);
        {
            QMutexLocker locker(&m_mutex);
            if (m_generating.contains(thumbnailPath)) continue;
            m_generating.insert(thumbnailPath);
        }
        tasks.append({sampleIndex, timestamps.at(sampleIndex), thumbnailPath});
    }
    if (tasks.isEmpty()) return;

    m_pool.start([this, videoPath, tasks] {
        if (!QDir().mkpath(thumbnailCacheDirectory())) {
            for (const SampleTask &task : tasks) {
                {
                    QMutexLocker locker(&m_mutex);
                    m_generating.remove(task.thumbnailPath);
                }
                emit sampleFailed(videoPath, task.index,
                                  QStringLiteral("Failed to create thumbnail cache"));
            }
            return;
        }

        for (const SampleTask &task : tasks) {
            const auto finish = [this, thumbnailPath = task.thumbnailPath] {
                QMutexLocker locker(&m_mutex);
                m_generating.remove(thumbnailPath);
            };
            if (isValidThumbnail(task.thumbnailPath)) {
                finish();
                emit sampleGenerated(videoPath, task.index, task.thumbnailPath);
                continue;
            }

            QString error;
            const bool succeeded = createThumbnail(
                videoPath, {task.timestampMs}, task.thumbnailPath, &error);
            finish();
            if (!succeeded) {
                emit sampleFailed(videoPath, task.index, error);
                continue;
            }
            emit sampleGenerated(videoPath, task.index, task.thumbnailPath);
        }
    }, 1);
}

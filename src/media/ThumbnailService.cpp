#include "media/ThumbnailService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
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
constexpr int BulkThumbnailWorkerCount = 2;

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
                     const QString &thumbnailPath, QString *error,
                     const std::atomic_bool *cancelRequested = nullptr,
                     bool *wasCanceled = nullptr)
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
        bool finished = false;
        if (cancelRequested) {
            QElapsedTimer processTimer;
            processTimer.start();
            while (process.state() != QProcess::NotRunning) {
                if (cancelRequested->load(std::memory_order_relaxed)) {
                    process.kill();
                    process.waitForFinished();
                    if (wasCanceled) *wasCanceled = true;
                    QFile::remove(temporaryPath);
                    if (error) *error = QStringLiteral("Thumbnail generation canceled");
                    return false;
                }
                if (process.waitForFinished(100)) {
                    finished = true;
                    break;
                }
                if (processTimer.elapsed() >= 30000) {
                    process.kill();
                    process.waitForFinished();
                    break;
                }
            }
            if (!finished && process.state() == QProcess::NotRunning) {
                finished = process.waitForFinished(0);
            }
        } else {
            finished = process.waitForFinished(30000);
        }
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
    m_bulkPool.setMaxThreadCount(BulkThumbnailWorkerCount);
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
    cancelGenerateAll();
    m_bulkPool.waitForDone();
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

bool ThumbnailService::generateAll(const QStringList &videoPaths)
{
    int workerCount = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_bulkRunning) return false;

        QSet<QString> uniquePaths;
        m_bulkQueue.clear();
        for (const QString &videoPath : videoPaths) {
            if (videoPath.isEmpty() || uniquePaths.contains(videoPath)) continue;
            uniquePaths.insert(videoPath);
            m_bulkQueue.enqueue(videoPath);
        }

        m_bulkTotal = m_bulkQueue.size();
        m_bulkCompleted = 0;
        m_bulkGenerated = 0;
        m_bulkSkipped = 0;
        m_bulkFailed = 0;
        m_bulkCancelRequested.store(false, std::memory_order_relaxed);
        m_bulkRunning = true;
        m_bulkWorkersRemaining = qMin(BulkThumbnailWorkerCount, m_bulkTotal);
        workerCount = m_bulkWorkersRemaining;
    }

    if (m_bulkTotal == 0) {
        {
            QMutexLocker locker(&m_mutex);
            m_bulkRunning = false;
        }
        emit bulkProgress(0, 0, 0, 0, 0);
        emit bulkFinished(0, 0, 0, 0, false);
        return true;
    }

    for (int worker = 0; worker < workerCount; ++worker) {
        m_bulkPool.start([this] { runBulkWorker(); });
    }
    return true;
}

void ThumbnailService::cancelGenerateAll()
{
    QMutexLocker locker(&m_mutex);
    if (!m_bulkRunning) return;
    m_bulkCancelRequested.store(true, std::memory_order_relaxed);
    m_bulkQueue.clear();
    m_generationChanged.wakeAll();
}

void ThumbnailService::runBulkWorker()
{
    while (true) {
        QString videoPath;
        QString thumbnailPath;
        {
            QMutexLocker locker(&m_mutex);
            if (m_bulkCancelRequested.load(std::memory_order_relaxed)
                || m_bulkQueue.isEmpty()) {
                break;
            }
            videoPath = m_bulkQueue.dequeue();
            thumbnailPath = cachedPath(videoPath);
            while (m_generating.contains(thumbnailPath)
                   && !m_bulkCancelRequested.load(std::memory_order_relaxed)) {
                m_generationChanged.wait(&m_mutex, 100);
            }
            if (m_bulkCancelRequested.load(std::memory_order_relaxed)) break;
            m_generating.insert(thumbnailPath);
        }

        bool skipped = isValidThumbnail(thumbnailPath);
        bool succeeded = skipped;
        bool wasCanceled = false;
        QString error;
        if (!skipped) {
            if (!QDir().mkpath(thumbnailCacheDirectory())) {
                error = QStringLiteral("Failed to create thumbnail cache");
            } else {
                succeeded = createThumbnail(videoPath, {120000, 0}, thumbnailPath, &error,
                                            &m_bulkCancelRequested, &wasCanceled);
            }
        }

        int completed = 0;
        int total = 0;
        int generated = 0;
        int skippedCount = 0;
        int failedCount = 0;
        {
            QMutexLocker locker(&m_mutex);
            m_generating.remove(thumbnailPath);
            m_generationChanged.wakeAll();
            ++m_bulkCompleted;
            if (skipped) {
                ++m_bulkSkipped;
            } else if (succeeded) {
                ++m_bulkGenerated;
            } else if (!wasCanceled) {
                ++m_bulkFailed;
            }
            completed = m_bulkCompleted;
            total = m_bulkTotal;
            generated = m_bulkGenerated;
            skippedCount = m_bulkSkipped;
            failedCount = m_bulkFailed;
        }

        if (succeeded && !skipped) emit this->generated(videoPath, thumbnailPath);
        if (!succeeded && !wasCanceled) emit failed(videoPath, error);
        emit bulkProgress(completed, total, generated, skippedCount, failedCount);
    }

    int total = 0;
    int generated = 0;
    int skipped = 0;
    int failed = 0;
    bool canceled = false;
    bool lastWorker = false;
    {
        QMutexLocker locker(&m_mutex);
        --m_bulkWorkersRemaining;
        lastWorker = m_bulkWorkersRemaining == 0;
        if (lastWorker) {
            total = m_bulkTotal;
            generated = m_bulkGenerated;
            skipped = m_bulkSkipped;
            failed = m_bulkFailed;
            canceled = m_bulkCancelRequested.load(std::memory_order_relaxed);
            m_bulkRunning = false;
            m_bulkQueue.clear();
        }
    }
    if (lastWorker) emit bulkFinished(total, generated, skipped, failed, canceled);
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
            m_generationChanged.wakeAll();
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
                    m_generationChanged.wakeAll();
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
                m_generationChanged.wakeAll();
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

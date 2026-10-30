#pragma once

#include <QObject>
#include <atomic>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QThreadPool>
#include <QWaitCondition>
#include <QList>
#include <QString>
#include <QStringList>

class ThumbnailService : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService() override;
    static QString cachedPath(const QString &videoPath);
    static QList<qint64> sampleTimestamps(qint64 durationMs);
    static QString sampledCachedPath(const QString &videoPath, int sampleIndex);
    void setThreadLimitEnabled(bool enabled);
    bool generateAll(const QStringList &videoPaths);
    void cancelGenerateAll();
    void generate(const QString &videoPath);
    void generateSamples(const QString &videoPath, qint64 durationMs);

signals:
    void generated(const QString &videoPath, const QString &thumbnailPath);
    void failed(const QString &videoPath, const QString &message);
    void bulkProgress(int completed, int total, int generated, int skipped, int failed);
    void bulkFinished(int total, int generated, int skipped, int failed, bool canceled);
    void sampleGenerated(const QString &videoPath, int sampleIndex, const QString &thumbnailPath);
    void sampleFailed(const QString &videoPath, int sampleIndex, const QString &message);

private:
    void runBulkWorker();

    QThreadPool m_pool;
    QThreadPool m_bulkPool;
    QMutex m_mutex;
    QWaitCondition m_generationChanged;
    QSet<QString> m_generating;
    QQueue<QString> m_bulkQueue;
    bool m_bulkRunning = false;
    std::atomic_bool m_bulkCancelRequested = false;
    int m_bulkWorkersRemaining = 0;
    int m_bulkTotal = 0;
    int m_bulkCompleted = 0;
    int m_bulkGenerated = 0;
    int m_bulkSkipped = 0;
    int m_bulkFailed = 0;
};

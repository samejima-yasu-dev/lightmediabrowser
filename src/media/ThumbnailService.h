#pragma once

#include <QObject>
#include <QMutex>
#include <QSet>
#include <QThreadPool>
#include <QList>
#include <QString>

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
    void generate(const QString &videoPath);
    void generateSamples(const QString &videoPath, qint64 durationMs);

signals:
    void generated(const QString &videoPath, const QString &thumbnailPath);
    void failed(const QString &videoPath, const QString &message);
    void sampleGenerated(const QString &videoPath, int sampleIndex, const QString &thumbnailPath);
    void sampleFailed(const QString &videoPath, int sampleIndex, const QString &message);

private:
    QThreadPool m_pool;
    QMutex m_mutex;
    QSet<QString> m_generating;
};

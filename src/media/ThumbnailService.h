#pragma once

#include <QObject>
#include <QThreadPool>
#include <QString>

class ThumbnailService : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService() override;
    static QString cachedPath(const QString &videoPath);
    void generate(const QString &videoPath);

signals:
    void generated(const QString &videoPath, const QString &thumbnailPath);
    void failed(const QString &videoPath, const QString &message);

private:
    QThreadPool m_pool;
};

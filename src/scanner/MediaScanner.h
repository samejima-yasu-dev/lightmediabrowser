#pragma once

#include "domain/Video.h"

#include <QObject>
#include <QFutureWatcher>
#include <QString>

class MediaScanner : public QObject
{
    Q_OBJECT
public:
    explicit MediaScanner(QObject *parent = nullptr);
    void scan(const QString &folder);
    void scanFolders(const QStringList &folders);

signals:
    void videoDiscovered(const Video &video);
    void progress(int count);
    void finished(int count);
    void error(const QString &message);

private:
    QFutureWatcher<QList<Video>> m_watcher;
};

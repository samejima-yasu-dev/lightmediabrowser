#pragma once

#include "domain/Video.h"

#include <QObject>
#include <QFutureWatcher>
#include <QList>
#include <QString>
#include <QStringList>

class MediaScanner : public QObject
{
    Q_OBJECT
public:
    explicit MediaScanner(QObject *parent = nullptr);
    ~MediaScanner() override;
    void scan(const QString &folder);
    void scanFolders(const QStringList &folders);

signals:
    void videoDiscovered(const Video &video);
    void progress(int count);
    void finished(int count, bool complete, const QStringList &errors);

private:
    struct ScanResult
    {
        QList<Video> videos;
        QStringList errors;
        bool complete = true;
    };

    QFutureWatcher<ScanResult> m_watcher;
};

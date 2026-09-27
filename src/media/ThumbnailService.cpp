#include "media/ThumbnailService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

ThumbnailService::ThumbnailService(QObject *parent) : QObject(parent)
{
    // CPU使用率が100%に張り付くのを防ぐため、並列生成スレッド数をコア数に応じて抑制（最大2スレッド）
    const int threadCount = qBound(1, QThread::idealThreadCount() / 2, 4);
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
    return cacheDir + QLatin1Char('/') + QString::fromLatin1(key) + QStringLiteral("-t60.jpg");
}

void ThumbnailService::generate(const QString &videoPath)
{
    m_pool.start([this, videoPath] {
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/thumbnails");
        if (!QDir().mkpath(cacheDir)) {
            emit failed(videoPath, QStringLiteral("Failed to create thumbnail cache"));
            return;
        }
        const QString thumbnailPath = cachedPath(videoPath);
        if (QFileInfo::exists(thumbnailPath)) {
            emit generated(videoPath, thumbnailPath);
            return;
        }

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
                QStringLiteral("-vf"), QStringLiteral("scale=320:-1"),
                thumbnailPath
            });
            process.start();
            return process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && QFileInfo::exists(thumbnailPath);
        };
        bool succeeded = runAt(QStringLiteral("00:02:00"));
        if (!succeeded) {
            QFile::remove(thumbnailPath);
            succeeded = runAt(QStringLiteral("00:00:00"));
        }
        if (!succeeded) {
            QString message = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (message.isEmpty()) message = process.errorString();
            emit failed(videoPath, message);
            return;
        }
        emit generated(videoPath, thumbnailPath);
    });
}

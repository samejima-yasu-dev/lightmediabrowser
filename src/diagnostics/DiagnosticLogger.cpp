#include "diagnostics/DiagnosticLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
constexpr qint64 MaxLogSize = 5 * 1024 * 1024;
constexpr int BackupCount = 3;
QString g_logPath;
QMutex g_logMutex;

QString backupPath(int index)
{
    return g_logPath + QStringLiteral(".%1").arg(index);
}

void rotateIfNeeded()
{
    QFile log(g_logPath);
    if (!log.exists() || log.size() < MaxLogSize) return;

    QFile::remove(backupPath(BackupCount));
    for (int index = BackupCount; index >= 2; --index) {
        QFile::rename(backupPath(index - 1), backupPath(index));
    }
    QFile::rename(g_logPath, backupPath(1));
}

void diagnosticMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context)
    QMutexLocker locker(&g_logMutex);
    rotateIfNeeded();

    QFile log(g_logPath);
    if (!log.open(QIODevice::Append | QIODevice::Text)) return;

    const QString level = type == QtCriticalMsg ? QStringLiteral("CRITICAL")
                          : type == QtWarningMsg ? QStringLiteral("WARNING")
                          : type == QtFatalMsg ? QStringLiteral("FATAL")
                          : type == QtInfoMsg ? QStringLiteral("INFO")
                          : QStringLiteral("DEBUG");
    QTextStream stream(&log);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
           << " [" << level << "] " << message << '\n';
    stream.flush();
}
}

QString diagnosticLogPath()
{
    return g_logPath;
}

bool installDiagnosticLogger(QString *logPath, QString *error)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory)) {
        if (error) *error = QStringLiteral("Could not create diagnostic directory: %1").arg(directory);
        return false;
    }

    g_logPath = directory + QStringLiteral("/lightmediabrowser.log");
    if (logPath) *logPath = g_logPath;

    QFile log(g_logPath);
    if (!log.open(QIODevice::Append | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not open diagnostic log: %1").arg(g_logPath);
        return false;
    }
    log.close();

    qInstallMessageHandler(diagnosticMessageHandler);
    return true;
}

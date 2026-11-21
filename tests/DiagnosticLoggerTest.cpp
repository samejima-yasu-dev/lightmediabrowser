#include "diagnostics/DiagnosticLogger.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QtTest>

bool logPathAndInstallationCreateFile()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserDiagnosticTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowserTests"));

    QString logPath;
    QString error;
    if (!installDiagnosticLogger(&logPath, &error)) return false;
    if (logPath.isEmpty() || logPath != diagnosticLogPath()) return false;

    qInfo() << "diagnostic logger smoke test";
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QByteArray contents = file.readAll();
    return contents.contains("diagnostic logger smoke test");
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    return logPathAndInstallationCreateFile() ? 0 : 1;
}

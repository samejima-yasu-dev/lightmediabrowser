#include "diagnostics/DiagnosticLogger.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QtTest>

void logPathAndInstallationCreateFile()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserDiagnosticTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowserTests"));

    QString logPath;
    QString error;
    QVERIFY(installDiagnosticLogger(&logPath, &error));
    QVERIFY(!logPath.isEmpty());
    QCOMPARE(logPath, diagnosticLogPath());

    qInfo() << "diagnostic logger smoke test";
    QFile file(logPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.contains("diagnostic logger smoke test"));
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    logPathAndInstallationCreateFile();
    return 0;
}

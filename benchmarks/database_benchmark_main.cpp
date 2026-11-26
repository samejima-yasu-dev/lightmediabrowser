#include "DatabaseBenchmark.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>

namespace
{
bool parseCounts(const QString &value, QList<int> *counts)
{
    if (!counts) return false;
    for (const QString &part : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int count = part.toInt(&ok);
        if (!ok) return false;
        counts->append(count);
    }
    return !counts->isEmpty();
}
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(resources);
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("LightMediaBrowser"));
    application.setOrganizationName(QStringLiteral("LightMediaBrowser"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Benchmark LightMediaBrowser database search."));
    parser.addHelpOption();
    const QCommandLineOption videoDirectoryOption(
        QStringList{QStringLiteral("v"), QStringLiteral("video-dir")},
        QStringLiteral("Directory containing the generated video files."),
        QStringLiteral("directory"));
    const QCommandLineOption countsOption(
        QStringList{QStringLiteral("c"), QStringLiteral("counts")},
        QStringLiteral("Comma-separated ascending row counts (default: 100,1000,10000)."),
        QStringLiteral("counts"),
        QStringLiteral("100,1000,10000"));
    parser.addOption(videoDirectoryOption);
    parser.addOption(countsOption);
    parser.process(application);

    if (parser.value(videoDirectoryOption).isEmpty()) {
        QTextStream(stderr) << "Missing --video-dir.\n";
        return 2;
    }

    QList<int> counts;
    if (!parseCounts(parser.value(countsOption), &counts)) {
        QTextStream(stderr) << "Invalid --counts list.\n";
        return 2;
    }

    QJsonObject report;
    QString error;
    if (!runDatabaseBenchmark(parser.value(videoDirectoryOption), counts, &report, &error)) {
        QTextStream(stderr) << error << '\n';
        return 1;
    }
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Indented);
    return 0;
}

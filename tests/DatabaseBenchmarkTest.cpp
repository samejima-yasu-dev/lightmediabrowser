#include "benchmarks/DatabaseBenchmark.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class DatabaseBenchmarkTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reportsScannedVideosAndSearchMeasurements();
    void refusesToReuseAnExistingDatabase();

private:
    bool createVideos(const QString &directory, int count) const;
};

void DatabaseBenchmarkTest::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowser"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowser"));
    Q_INIT_RESOURCE(resources);
}

bool DatabaseBenchmarkTest::createVideos(const QString &directory, int count) const
{
    if (!QDir().mkpath(directory)) return false;
    for (int i = 1; i <= count; ++i) {
        QFile video(QDir(directory).filePath(QStringLiteral("sample-%1.mp4").arg(i, 5, 10, QLatin1Char('0'))));
        if (!video.open(QIODevice::WriteOnly)) return false;
        if (video.write("sample") != 6) return false;
    }
    return true;
}

void DatabaseBenchmarkTest::reportsScannedVideosAndSearchMeasurements()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString dataHome = temporaryDirectory.filePath(QStringLiteral("data-home"));
    QVERIFY(qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit()));
    QVERIFY(createVideos(temporaryDirectory.filePath(QStringLiteral("videos")), 3));

    QJsonObject report;
    QString error;
    QVERIFY2(runDatabaseBenchmark(temporaryDirectory.filePath(QStringLiteral("videos")), {1, 3},
                                  &report, &error),
             qPrintable(error));

    QCOMPARE(report.value(QStringLiteral("scanned_video_count")).toInt(), 3);
    QVERIFY(report.value(QStringLiteral("database_path")).toString().startsWith(dataHome));
    QVERIFY(report.contains(QStringLiteral("scan_ms")));
    QVERIFY(report.value(QStringLiteral("scan_ms")).toDouble() >= 0.0);
    QVERIFY(!report.value(QStringLiteral("qt_version")).toString().isEmpty());
    QVERIFY(!report.value(QStringLiteral("sqlite_version")).toString().isEmpty());
    QVERIFY(!report.value(QStringLiteral("build_type")).toString().isEmpty());
    const QJsonArray points = report.value(QStringLiteral("points")).toArray();
    QCOMPARE(points.size(), 2);
    QCOMPARE(points.at(0).toObject().value(QStringLiteral("video_count")).toInt(), 1);
    QCOMPARE(points.at(1).toObject().value(QStringLiteral("video_count")).toInt(), 3);
    QCOMPARE(points.at(1).toObject().value(QStringLiteral("search_result_count")).toInt(), 1);
    QVERIFY(points.at(1).toObject().value(QStringLiteral("search_median_ms")).toDouble() >= 0.0);
    QVERIFY(points.at(1).toObject().value(QStringLiteral("count_median_ms")).toDouble() >= 0.0);
}

void DatabaseBenchmarkTest::refusesToReuseAnExistingDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString dataHome = temporaryDirectory.filePath(QStringLiteral("data-home"));
    QVERIFY(qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit()));
    QVERIFY(createVideos(temporaryDirectory.filePath(QStringLiteral("videos")), 1));

    QJsonObject report;
    QString error;
    const QString videoDirectory = temporaryDirectory.filePath(QStringLiteral("videos"));
    QVERIFY2(runDatabaseBenchmark(videoDirectory, {1}, &report, &error), qPrintable(error));

    const QString databasePath = report.value(QStringLiteral("database_path")).toString();
    const QDateTime modifiedBefore = QFileInfo(databasePath).lastModified();
    QVERIFY(!runDatabaseBenchmark(videoDirectory, {1}, &report, &error));
    QVERIFY(error.contains(QStringLiteral("already exists")));
    QCOMPARE(QFileInfo(databasePath).lastModified(), modifiedBefore);
}

QTEST_GUILESS_MAIN(DatabaseBenchmarkTest)
#include "DatabaseBenchmarkTest.moc"

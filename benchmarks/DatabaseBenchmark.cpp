#include "DatabaseBenchmark.h"

#include "database/Database.h"
#include "scanner/MediaScanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <functional>

#ifndef LMB_BENCHMARK_BUILD_TYPE
#define LMB_BENCHMARK_BUILD_TYPE "unknown"
#endif

namespace
{
constexpr int MaximumVideoCount = 10'000;
constexpr int SearchSamples = 21;
constexpr int PageSize = 100;

bool setError(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

double medianMilliseconds(const std::function<bool()> &operation)
{
    QList<double> samples;
    for (int i = 0; i <= SearchSamples; ++i) {
        QElapsedTimer timer;
        timer.start();
        if (!operation()) {
            return -1.0;
        }
        const double elapsed = static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0;
        if (i > 0) samples.append(elapsed);
    }

    std::sort(samples.begin(), samples.end());
    return samples.at(samples.size() / 2);
}
}

bool runDatabaseBenchmark(const QString &videoDirectory, const QList<int> &counts,
                          QJsonObject *report, QString *error)
{
    if (!report) return setError(error, QStringLiteral("Benchmark report output is unavailable."));

    const QString xdgDataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (xdgDataHome.isEmpty() || !QDir::isAbsolutePath(xdgDataHome)) {
        return setError(error, QStringLiteral("Set XDG_DATA_HOME to an absolute, isolated directory."));
    }

    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowser"));
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowser"));

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString relativeDataDirectory = QDir(xdgDataHome).relativeFilePath(dataDirectory);
    if (dataDirectory.isEmpty() || relativeDataDirectory == QStringLiteral("..") ||
        relativeDataDirectory.startsWith(QStringLiteral("../")) ||
        QDir::isAbsolutePath(relativeDataDirectory)) {
        return setError(error, QStringLiteral("Application data path is outside XDG_DATA_HOME."));
    }
    const QString databasePath = QDir(dataDirectory).filePath(QStringLiteral("database.sqlite"));
    if (QFileInfo::exists(databasePath) || QFileInfo::exists(databasePath + QStringLiteral("-wal")) ||
        QFileInfo::exists(databasePath + QStringLiteral("-shm"))) {
        return setError(error, QStringLiteral("Benchmark database already exists: %1").arg(databasePath));
    }

    if (counts.isEmpty() || counts.size() > 10) {
        return setError(error, QStringLiteral("Provide between one and ten benchmark sizes."));
    }
    int previousCount = 0;
    for (const int count : counts) {
        if (count <= previousCount || count > MaximumVideoCount) {
            return setError(error, QStringLiteral("Benchmark sizes must increase from 1 to %1.")
                                              .arg(MaximumVideoCount));
        }
        previousCount = count;
    }

    QList<Video> videos;
    int finishedCount = 0;
    bool scanComplete = false;
    QStringList scanErrors;
    MediaScanner scanner;
    QEventLoop scanLoop;
    QObject::connect(&scanner, &MediaScanner::videosDiscovered, &scanLoop,
                     [&videos](const QList<Video> &discovered) { videos = discovered; });
    QObject::connect(&scanner, &MediaScanner::finished, &scanLoop,
                     [&scanLoop, &finishedCount, &scanComplete, &scanErrors]
                     (int count, bool complete, const QStringList &errors) {
        finishedCount = count;
        scanComplete = complete;
        scanErrors = errors;
        scanLoop.quit();
    });

    QElapsedTimer scanTimer;
    scanTimer.start();
    scanner.scan(videoDirectory);
    scanLoop.exec();
    const double scanMilliseconds = static_cast<double>(scanTimer.nsecsElapsed()) / 1'000'000.0;
    if (!scanComplete) {
        return setError(error, QStringLiteral("Video scan was incomplete: %1").arg(scanErrors.join(QStringLiteral("; "))));
    }
    if (finishedCount != videos.size() || videos.size() != counts.last()) {
        return setError(error, QStringLiteral("Expected %1 videos, but scanned %2.")
                                          .arg(counts.last()).arg(videos.size()));
    }

    Database database;
    QString databaseError;
    if (!database.open(&databaseError)) {
        return setError(error, QStringLiteral("Could not create benchmark database: %1").arg(databaseError));
    }

    QSqlQuery sqliteVersionQuery(QSqlDatabase::database(QStringLiteral("lightmediabrowser"), false));
    if (!sqliteVersionQuery.exec(QStringLiteral("SELECT sqlite_version()")) ||
        !sqliteVersionQuery.next()) {
        return setError(error, QStringLiteral("Could not read SQLite version: %1")
                                          .arg(sqliteVersionQuery.lastError().text()));
    }

    QJsonArray points;
    int previous = 0;
    for (const int count : counts) {
        QList<Video> batch;
        for (int i = previous; i < count; ++i) batch.append(videos.at(i));

        QElapsedTimer insertTimer;
        insertTimer.start();
        if (!database.upsertVideos(batch, &databaseError)) {
            return setError(error, QStringLiteral("Could not register benchmark videos: %1").arg(databaseError));
        }
        const double insertMilliseconds = static_cast<double>(insertTimer.nsecsElapsed()) / 1'000'000.0;

        const QString queryText = videos.at(count - 1).fileName;
        int searchResultCount = 0;
        const auto search = [&database, &queryText, &searchResultCount] {
            const QList<VideoLightItem> results = database.searchVideosLight(
                queryText, 0, false, 0, false, {}, PageSize, 0);
            searchResultCount = results.size();
            return searchResultCount == 1;
        };
        const double searchMedian = medianMilliseconds(search);
        if (searchMedian < 0.0) {
            return setError(error, QStringLiteral("Search did not return exactly one matching video."));
        }

        const auto countSearch = [&database, &queryText] {
            return database.countVideosLight(queryText) == 1;
        };
        const double countMedian = medianMilliseconds(countSearch);
        if (countMedian < 0.0) {
            return setError(error, QStringLiteral("Search count did not return exactly one matching video."));
        }

        const int expectedPageSize = qMin(count, PageSize);
        const auto loadPage = [&database, expectedPageSize] {
            return database.searchVideosLight({}, 0, false, 0, false, {}, PageSize, 0).size() == expectedPageSize;
        };
        const double pageMedian = medianMilliseconds(loadPage);
        if (pageMedian < 0.0) {
            return setError(error, QStringLiteral("Library page returned an unexpected number of videos."));
        }

        QJsonObject point;
        point.insert(QStringLiteral("video_count"), count);
        point.insert(QStringLiteral("batch_insert_ms"), insertMilliseconds);
        point.insert(QStringLiteral("search_result_count"), searchResultCount);
        point.insert(QStringLiteral("search_median_ms"), searchMedian);
        point.insert(QStringLiteral("count_median_ms"), countMedian);
        point.insert(QStringLiteral("page_size"), expectedPageSize);
        point.insert(QStringLiteral("page_median_ms"), pageMedian);
        points.append(point);
        previous = count;
    }

    report->insert(QStringLiteral("database_path"), databasePath);
    report->insert(QStringLiteral("qt_version"), QString::fromLatin1(qVersion()));
    report->insert(QStringLiteral("sqlite_version"), sqliteVersionQuery.value(0).toString());
    const QString buildType = QString::fromLatin1(LMB_BENCHMARK_BUILD_TYPE);
    report->insert(QStringLiteral("build_type"),
                   buildType.isEmpty() ? QStringLiteral("unspecified") : buildType);
    report->insert(QStringLiteral("scanned_video_count"), finishedCount);
    report->insert(QStringLiteral("scan_ms"), scanMilliseconds);
    report->insert(QStringLiteral("search_samples"), SearchSamples);
    report->insert(QStringLiteral("points"), points);
    return true;
}

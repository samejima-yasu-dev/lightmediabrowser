#include "database/Database.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QtTest>

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void batchUpsertAddsAndUpdatesVideos();
    void batchUpsertRollsBackWhenAnyVideoFails();
    void duplicateFilterMatchesFileSizeOrFileName();
    void futureSchemaVersionIsRejected();
    void initialSchemaMigratesToCurrentVersion();
    void declaredCurrentSchemaMissingTablesIsRejected();
};

void DatabaseTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserDatabaseTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowserTests"));
}

void DatabaseTest::batchUpsertAddsAndUpdatesVideos()
{
    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = QStringLiteral("batch-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString firstPath = temporaryDirectory.filePath(QStringLiteral("first.mp4"));
    const QString secondPath = temporaryDirectory.filePath(QStringLiteral("second.mp4"));
    const QString thirdPath = temporaryDirectory.filePath(QStringLiteral("third.mp4"));

    Video first;
    first.filePath = firstPath;
    first.fileName = marker + QStringLiteral("-first.mp4");
    first.fileSize = 100;

    Video second;
    second.filePath = secondPath;
    second.fileName = marker + QStringLiteral("-second.mp4");
    second.fileSize = 200;

    QVERIFY2(database.upsertVideos({first, second}, &error), qPrintable(error));

    first.fileName = marker + QStringLiteral("-renamed.mp4");
    first.fileSize = 300;
    Video third;
    third.filePath = thirdPath;
    third.fileName = marker + QStringLiteral("-third.mp4");
    third.fileSize = 400;
    QVERIFY2(database.upsertVideos({first, third}, &error), qPrintable(error));

    const QList<VideoLightItem> videos = database.searchVideosLight(marker);
    QCOMPARE(videos.size(), 3);
    bool foundUpdatedVideo = false;
    for (const VideoLightItem &video : videos) {
        if (video.filePath == firstPath) {
            QCOMPARE(video.fileName, marker + QStringLiteral("-renamed.mp4"));
            QCOMPARE(video.fileSize, qint64(300));
            foundUpdatedVideo = true;
        }
    }
    QVERIFY(foundUpdatedVideo);
}

void DatabaseTest::batchUpsertRollsBackWhenAnyVideoFails()
{
    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = QStringLiteral("rollback-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));

    Video valid;
    valid.filePath = temporaryDirectory.filePath(QStringLiteral("valid.mp4"));
    valid.fileName = marker + QStringLiteral("-valid.mp4");

    Video invalid;
    invalid.filePath = temporaryDirectory.filePath(QStringLiteral("invalid.mp4"));

    QVERIFY(!database.upsertVideos({valid, invalid}, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(database.countVideosLight(marker), 0);
}

void DatabaseTest::futureSchemaVersionIsRejected()
{
    const QString previousName = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserFutureSchemaTest"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir().mkpath(dataDir));
    const QString databasePath = dataDir + QStringLiteral("/database.sqlite");

    const QString connectionName = QStringLiteral("future-schema-test");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 6")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    Database database;
    QString error;
    QVERIFY(!database.open(&error));
    QVERIFY(error.contains(QStringLiteral("newer than this application supports")));
    QCoreApplication::setApplicationName(previousName);
}

void DatabaseTest::initialSchemaMigratesToCurrentVersion()
{
    const QString previousName = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserMigrationTest"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir().mkpath(dataDir));
    const QString databasePath = dataDir + QStringLiteral("/database.sqlite");

    const QString connectionName = QStringLiteral("initial-schema-test");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 0")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    const QString verifyConnectionName = QStringLiteral("verify-migration-test");
    {
        QSqlDatabase verify = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnectionName);
        verify.setDatabaseName(databasePath);
        QVERIFY(verify.open());
        QSqlQuery version(verify);
        QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(version.next());
        QCOMPARE(version.value(0).toInt(), 5);

        QSqlQuery tables(verify);
        QVERIFY(tables.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name IN ('videos','categories','tags','video_tags')")));
        QStringList tableNames;
        while (tables.next()) tableNames.append(tables.value(0).toString());
        QVERIFY(tableNames.contains(QStringLiteral("videos")));
        QVERIFY(tableNames.contains(QStringLiteral("categories")));
        QVERIFY(tableNames.contains(QStringLiteral("tags")));
        QVERIFY(tableNames.contains(QStringLiteral("video_tags")));
        verify.close();
    }
    QSqlDatabase::removeDatabase(verifyConnectionName);
    QCoreApplication::setApplicationName(previousName);
}

void DatabaseTest::declaredCurrentSchemaMissingTablesIsRejected()
{
    const QString previousName = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserIncompleteSchemaTest"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir().mkpath(dataDir));
    const QString databasePath = dataDir + QStringLiteral("/database.sqlite");

    const QString connectionName = QStringLiteral("incomplete-schema-test");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 5")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    Database database;
    QString error;
    QVERIFY(!database.open(&error));
    QVERIFY(error.contains(QStringLiteral("schema")));
    QCoreApplication::setApplicationName(previousName);
}

void DatabaseTest::duplicateFilterMatchesFileSizeOrFileName()
{
    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = QStringLiteral("duplicates-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool sizeOk = false;
    const qint64 uniqueSize = QUuid::createUuid().toString(QUuid::Id128)
                                  .left(15).toLongLong(&sizeOk, 16);
    QVERIFY(sizeOk);

    Video sameSizeFirst;
    sameSizeFirst.filePath = temporaryDirectory.filePath(QStringLiteral("same-size-first.mp4"));
    sameSizeFirst.fileName = marker + QStringLiteral("-first.mp4");
    sameSizeFirst.fileSize = uniqueSize;
    sameSizeFirst.note = marker;

    Video sameSizeSecond;
    sameSizeSecond.filePath = temporaryDirectory.filePath(QStringLiteral("same-size-second.mp4"));
    sameSizeSecond.fileName = marker + QStringLiteral("-second.mp4");
    sameSizeSecond.fileSize = uniqueSize;
    sameSizeSecond.note = marker;

    Video sameNameDifferentSize;
    sameNameDifferentSize.filePath = temporaryDirectory.filePath(QStringLiteral("same-name.mp4"));
    sameNameDifferentSize.fileName = marker + QStringLiteral("-first.mp4");
    sameNameDifferentSize.fileSize = uniqueSize + 1;
    sameNameDifferentSize.note = marker;

    Video unique;
    unique.filePath = temporaryDirectory.filePath(QStringLiteral("unique.mp4"));
    unique.fileName = marker + QStringLiteral("-unique.mp4");
    unique.fileSize = uniqueSize + 2;
    unique.note = marker;

    QVERIFY2(database.upsertVideos(
                 {sameSizeFirst, sameSizeSecond, sameNameDifferentSize, unique}, &error),
             qPrintable(error));

    const QList<VideoLightItem> duplicates = database.searchVideosLight(
        marker, 0, false, 0, true);
    QCOMPARE(duplicates.size(), 3);
    QCOMPARE(database.countVideosLight(marker, 0, false, 0, true), 3);
}

QTEST_GUILESS_MAIN(DatabaseTest)
#include "DatabaseTest.moc"

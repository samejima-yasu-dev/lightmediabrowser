#include "database/Database.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void batchUpsertAddsAndUpdatesVideos();
    void batchUpsertRollsBackWhenAnyVideoFails();
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

QTEST_GUILESS_MAIN(DatabaseTest)
#include "DatabaseTest.moc"

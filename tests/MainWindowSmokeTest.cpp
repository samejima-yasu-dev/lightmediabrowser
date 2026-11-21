#include "ui/MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QtTest>

class MainWindowSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void startupAndFilterControlsAreWired();
};

void MainWindowSmokeTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserSmokeTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowserTests"));
}

void MainWindowSmokeTest::startupAndFilterControlsAreWired()
{
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();

    auto *search = window.findChild<QLineEdit *>(QStringLiteral("searchLineEdit"));
    auto *folder = window.findChild<QComboBox *>(QStringLiteral("folderCombo"));
    auto *libraryFilter = window.findChild<QComboBox *>(QStringLiteral("libraryFilterCombo"));
    auto *duplicateFilter = window.findChild<QComboBox *>(QStringLiteral("duplicateFilterCombo"));
    auto *sort = window.findChild<QComboBox *>(QStringLiteral("sortSelectorCombo"));
    auto *plays = window.findChild<QSpinBox *>(QStringLiteral("minPlayCountSpin"));
    auto *tags = window.findChild<QTreeWidget *>(QStringLiteral("tagTree"));
    auto *videos = window.findChild<QListWidget *>(QStringLiteral("videoList"));

    QVERIFY(search);
    QVERIFY(folder);
    QVERIFY(libraryFilter);
    QVERIFY(duplicateFilter);
    QVERIFY(sort);
    QVERIFY(plays);
    QVERIFY(tags);
    QVERIFY(videos);

    search->setText(QStringLiteral("offscreen-smoke-no-match"));
    QTest::qWait(300);
    libraryFilter->setCurrentIndex(1);
    duplicateFilter->setCurrentIndex(1);
    sort->setCurrentIndex(2);
    plays->setValue(1);
    QCoreApplication::processEvents();

    QCOMPARE(search->text(), QStringLiteral("offscreen-smoke-no-match"));
    QCOMPARE(libraryFilter->currentIndex(), 1);
    QCOMPARE(duplicateFilter->currentIndex(), 1);
    QCOMPARE(sort->currentIndex(), 2);
    QCOMPARE(plays->value(), 1);
}

QTEST_MAIN(MainWindowSmokeTest)
#include "MainWindowSmokeTest.moc"

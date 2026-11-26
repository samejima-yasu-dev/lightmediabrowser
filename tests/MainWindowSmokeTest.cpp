#include "ui/MainWindow.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

class MainWindowSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void startupAndFilterControlsAreWired();
    void tagDataImportAndExportActionsAreAvailable();
    void tagDataMenuIsAvailableFromSingleToolbarButton();
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

void MainWindowSmokeTest::tagDataImportAndExportActionsAreAvailable()
{
    MainWindow window;

    QAction *exportAction = window.findChild<QAction *>(QStringLiteral("exportTagDataAction"));
    QAction *importAction = window.findChild<QAction *>(QStringLiteral("importTagDataAction"));
    QVERIFY(exportAction);
    QVERIFY(importAction);
}

void MainWindowSmokeTest::tagDataMenuIsAvailableFromSingleToolbarButton()
{
    MainWindow window;
    auto *tagDataButton = window.findChild<QToolButton *>(QStringLiteral("tagDataButton"));

    QVERIFY(tagDataButton);
    QCOMPARE(tagDataButton->popupMode(), QToolButton::InstantPopup);
    QVERIFY(tagDataButton->menu());
    QCOMPARE(tagDataButton->menu()->actions().size(), 2);
    QVERIFY(window.menuBar()->actions().isEmpty());
}

QTEST_MAIN(MainWindowSmokeTest)
#include "MainWindowSmokeTest.moc"

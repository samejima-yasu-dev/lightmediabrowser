#include "database/Database.h"
#include "ui/TagManager.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QUuid>
#include <QtTest>

class TagManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void refreshShowsCategoryAndTag();
    void deleteTagRemovesItFromTree();
};

void TagManagerTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("LightMediaBrowserTagManagerTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("LightMediaBrowserTests"));
}

void TagManagerTest::refreshShowsCategoryAndTag()
{
    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(database.addCategory(QStringLiteral("Category-")+suffix, &error));
    QVERIFY(database.addTag(QStringLiteral("Category-")+suffix,
                           QStringLiteral("Tag-")+suffix, &error));

    QWidget parent;
    QTreeWidget tree;
    TagManager manager(&database, &tree, &parent);
    manager.refresh();

    QTreeWidgetItem *category = nullptr;
    const QString categoryName = QStringLiteral("Category-")+suffix;
    for (int index = 0; index < tree.topLevelItemCount(); ++index) {
        if (tree.topLevelItem(index)->text(0) == categoryName) {
            category = tree.topLevelItem(index);
            break;
        }
    }
    QVERIFY(category);
    QCOMPARE(category->childCount(), 1);
    QCOMPARE(category->child(0)->text(0), QStringLiteral("Tag-")+suffix);
}

void TagManagerTest::deleteTagRemovesItFromTree()
{
    Database database;
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));

    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString categoryName = QStringLiteral("DeleteCategory-")+suffix;
    const QString tagName = QStringLiteral("DeleteTag-")+suffix;
    QVERIFY(database.addCategory(categoryName, &error));
    QVERIFY(database.addTag(categoryName, tagName, &error));

    QWidget parent;
    QTreeWidget tree;
    TagManager manager(&database, &tree, &parent);
    manager.refresh();

    QTreeWidgetItem *category = nullptr;
    for (int index = 0; index < tree.topLevelItemCount(); ++index) {
        if (tree.topLevelItem(index)->text(0) == categoryName) {
            category = tree.topLevelItem(index);
            break;
        }
    }
    QVERIFY(category);
    for (int index = 0; index < category->childCount(); ++index) {
        if (category->child(index)->text(0) == tagName) {
            const qint64 tagId = category->child(index)->data(0, Qt::UserRole).toLongLong();
            QVERIFY(tagId > 0);
            QVERIFY(manager.deleteTag(tagId));
            break;
        }
    }

    QVERIFY(database.tags().contains(tagName) == false);
}

QTEST_MAIN(TagManagerTest)
#include "TagManagerTest.moc"

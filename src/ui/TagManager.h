#pragma once

#include <QObject>
#include <QPoint>
#include <QString>

class Database;
class QTreeWidget;
class QWidget;

class TagManager : public QObject
{
    Q_OBJECT
public:
    TagManager(Database *database, QTreeWidget *tree, QWidget *parent = nullptr);

    void refresh();
    void showContextMenu(const QPoint &position);
    void assignToVideo(qint64 videoId, const QString &videoName);
    bool addCategory(const QString &name);
    bool addTag(const QString &categoryName, const QString &tagName);
    bool deleteCategory(const QString &name);
    bool deleteTag(qint64 tagId);

signals:
    void tagSelectionChanged(qint64 tagId);
    void libraryChanged();

private:
    Database *m_database;
    QTreeWidget *m_tree;
};

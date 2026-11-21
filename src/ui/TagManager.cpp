#include "ui/TagManager.h"

#include "database/Database.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

TagManager::TagManager(Database *database, QTreeWidget *tree, QWidget *parent)
    : QObject(parent), m_database(database), m_tree(tree)
{
}

void TagManager::refresh()
{
    m_tree->clear();

    QMap<QString, QTreeWidgetItem *> categories;
    for (const TagInfo &tag : m_database->tagsWithCategories()) {
        if (!categories.contains(tag.category)) {
            categories.insert(tag.category, new QTreeWidgetItem(m_tree, {tag.category}));
        }
        if (!tag.name.isEmpty()) {
            auto *item = new QTreeWidgetItem(categories.value(tag.category), {tag.name});
            item->setData(0, Qt::UserRole, tag.id);
        }
    }

    for (QTreeWidgetItem *category : categories) {
        category->setExpanded(true);
    }
}

bool TagManager::addCategory(const QString &name)
{
    QString error;
    if (!m_database->addCategory(name, &error)) {
        QMessageBox::warning(qobject_cast<QWidget *>(parent()), QStringLiteral("Error"),
                             QStringLiteral("Could not add category: %1").arg(error));
        return false;
    }
    refresh();
    emit libraryChanged();
    return true;
}

bool TagManager::addTag(const QString &categoryName, const QString &tagName)
{
    QString error;
    if (!m_database->addTag(categoryName, tagName, &error)) {
        QMessageBox::warning(qobject_cast<QWidget *>(parent()), QStringLiteral("Error"),
                             QStringLiteral("Could not add tag: %1").arg(error));
        return false;
    }
    refresh();
    emit libraryChanged();
    return true;
}

bool TagManager::deleteCategory(const QString &name)
{
    QString error;
    if (!m_database->deleteCategory(name, &error)) {
        QMessageBox::warning(qobject_cast<QWidget *>(parent()), QStringLiteral("Error"),
                             QStringLiteral("Could not delete category: %1").arg(error));
        return false;
    }
    refresh();
    emit libraryChanged();
    return true;
}

bool TagManager::deleteTag(qint64 tagId)
{
    QString error;
    if (!m_database->deleteTag(tagId, &error)) {
        QMessageBox::warning(qobject_cast<QWidget *>(parent()), QStringLiteral("Error"),
                             QStringLiteral("Could not delete tag: %1").arg(error));
        return false;
    }
    refresh();
    emit libraryChanged();
    return true;
}

void TagManager::showContextMenu(const QPoint &position)
{
    QTreeWidgetItem *item = m_tree->itemAt(position);
    QMenu menu(qobject_cast<QWidget *>(parent()));

    if (!item) {
        menu.addAction(QStringLiteral("＋ Add Category"), this, [this] {
            bool ok = false;
            QWidget *owner = qobject_cast<QWidget *>(parent());
            QString name = QInputDialog::getText(owner, QStringLiteral("Add Category"),
                                                 QStringLiteral("Category name:"),
                                                 QLineEdit::Normal, QString(), &ok);
            if (ok) addCategory(name.trimmed());
        });
    } else if (item->parent() == nullptr) {
        const QString categoryName = item->text(0);
        menu.addAction(QStringLiteral("＋ Add Tag to \"%1\"").arg(categoryName), this, [this, categoryName] {
            bool ok = false;
            QWidget *owner = qobject_cast<QWidget *>(parent());
            QString name = QInputDialog::getText(owner, QStringLiteral("Add Tag"),
                                                 QStringLiteral("Tag name:"),
                                                 QLineEdit::Normal, QString(), &ok);
            if (ok) addTag(categoryName, name.trimmed());
        });
        menu.addSeparator();
        menu.addAction(QStringLiteral("🗑️ Delete Category \"%1\"").arg(categoryName), this, [this, categoryName] {
            QWidget *owner = qobject_cast<QWidget *>(parent());
            if (QMessageBox::question(owner, QStringLiteral("Delete Category"),
                                      QStringLiteral("Are you sure you want to delete category '%1' and all its tags?").arg(categoryName))
                == QMessageBox::Yes) {
                deleteCategory(categoryName);
            }
        });
    } else {
        const qint64 tagId = item->data(0, Qt::UserRole).toLongLong();
        const QString tagName = item->text(0);
        menu.addAction(QStringLiteral("🗑️ Delete Tag \"%1\"").arg(tagName), this, [this, tagId, tagName] {
            QWidget *owner = qobject_cast<QWidget *>(parent());
            if (QMessageBox::question(owner, QStringLiteral("Delete Tag"),
                                      QStringLiteral("Are you sure you want to delete tag '%1'?").arg(tagName))
                == QMessageBox::Yes) {
                deleteTag(tagId);
            }
        });
    }

    menu.exec(m_tree->viewport()->mapToGlobal(position));
}

void TagManager::assignToVideo(qint64 videoId, const QString &videoName)
{
    QWidget *owner = qobject_cast<QWidget *>(parent());
    QDialog dialog(owner);
    dialog.setWindowTitle(QStringLiteral("Assign Tags: %1").arg(videoName));
    dialog.resize(360, 480);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("Select tags for this video:"), &dialog));

    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    auto *scrollWidget = new QWidget();
    auto *scrollLayout = new QVBoxLayout(scrollWidget);

    const QList<TagInfo> allTags = m_database->tagsWithCategories();
    const QList<qint64> assignedTags = m_database->tagIdsForVideo(videoId);
    QMap<qint64, QCheckBox *> checkBoxes;

    for (const TagInfo &tag : allTags) {
        if (tag.id <= 0) continue;
        auto *check = new QCheckBox(QStringLiteral("[%1] %2").arg(tag.category, tag.name), scrollWidget);
        check->setProperty("tagId", tag.id);
        check->setChecked(assignedTags.contains(tag.id));
        scrollLayout->addWidget(check);
        checkBoxes.insert(tag.id, check);
    }
    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    layout->addWidget(scrollArea);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) return;

    QList<qint64> selectedTagIds;
    for (auto it = checkBoxes.cbegin(); it != checkBoxes.cend(); ++it) {
        if (it.value()->isChecked()) selectedTagIds.append(it.key());
    }

    QString error;
    if (!m_database->setVideoTags(videoId, selectedTagIds, &error)) {
        QMessageBox::warning(owner, QStringLiteral("Error"),
                             QStringLiteral("Could not assign tags: %1").arg(error));
        return;
    }
    emit libraryChanged();
}

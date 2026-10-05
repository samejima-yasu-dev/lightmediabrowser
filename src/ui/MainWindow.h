#pragma once

#include "database/Database.h"
#include "media/ThumbnailService.h"
#include "scanner/MediaScanner.h"

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QTreeWidget;
class QLabel;
class QListWidgetItem;
class QTreeWidgetItem;
class QComboBox;
class QPoint;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void addFolder();
    void removeFolder();
    void scanFolder();
    void refreshVideos();
    void showDetails(QListWidgetItem *item);
    void playVideo(QListWidgetItem *item);
    void toggleFavorite();
    void showVideoContextMenu(const QPoint &position);
    void showTagContextMenu(const QPoint &position);
    void assignTagsToVideo();
    void selectTag(QTreeWidgetItem *item, int column);
    void showScanResult(int count);

private:
    void buildUi();
    void refreshTags();
    void loadFolders();
    void saveFolders() const;
    void loadMoreVideos();
    void removeMissingVideosFromAvailableFolders();
    QString trText(const QString &key) const;

    Database m_database;
    ThumbnailService m_thumbnailService;
    MediaScanner m_scanner;
    QLineEdit *m_search = nullptr;
    QComboBox *m_folderSelector = nullptr;
    QComboBox *m_libraryFilter = nullptr;
    QComboBox *m_duplicateFilter = nullptr;
    QComboBox *m_sortSelector = nullptr;
    QSpinBox *m_minPlayCount = nullptr;
    QTreeWidget *m_tagTree = nullptr;
    QListWidget *m_videoList = nullptr;
    QLabel *m_details = nullptr;
    QString m_currentFolder;
    QString m_selectedTag;
    QStringList m_folders;
    QHash<QString, QString> m_thumbnailPaths;
    QHash<QString, QListWidgetItem *> m_videoItemsByPath;
    QList<VideoLightItem> m_allVisibleVideos;
    int m_loadedVideoCount = 0;
    const int m_pageSize = 100;
    bool m_databaseReady = false;
};

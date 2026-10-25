#pragma once

#include "database/Database.h"
#include "media/ThumbnailService.h"
#include "scanner/MediaScanner.h"

#include <QMainWindow>
#include <QHash>
#include <QElapsedTimer>
#include <QList>
#include <QSet>
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
    void showScanResult(int count, bool complete, const QStringList &errors);

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
    QList<QLabel *> m_detailSampleLabels;
    QString m_currentFolder;
    QString m_selectedVideoPath;
    qint64 m_selectedTagId = 0;
    QStringList m_folders;
    QHash<QString, QString> m_thumbnailPaths;
    QHash<QString, QListWidgetItem *> m_videoItemsByPath;
    int m_loadedVideoCount = 0;
    int m_nextVideoOffset = 0;
    int m_totalVideoCount = 0;
    int m_scanRegistrationCount = 0;
    QSet<qint64> m_metadataProbeIds;
    QElapsedTimer m_scanRegistrationTimer;
    bool m_pageLoadScheduled = false;
    quint64 m_listingGeneration = 0;
    const int m_pageSize = 100;
    bool m_databaseReady = false;
};

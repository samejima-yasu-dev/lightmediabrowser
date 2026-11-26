#include "database/Database.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <cmath>

namespace
{
struct ImportedTag
{
    QString category;
    QString name;
};

struct ImportedCategory
{
    QString name;
    QStringList tags;
};

struct ImportedVideo
{
    QString path;
    QString fileName;
    qint64 fileSize = 0;
    QString note;
    QList<ImportedTag> tags;
};

struct ParsedTagData
{
    QList<ImportedCategory> categories;
    QList<ImportedVideo> videos;
};

bool setError(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

QString tagKey(const QString &category, const QString &name)
{
    return category + QChar::Null + name;
}

bool parseTagData(const QJsonDocument &document, ParsedTagData *data, QString *error)
{
    if (!data) return setError(error, QStringLiteral("Tag data output is unavailable."));
    if (!document.isObject()) return setError(error, QStringLiteral("Tag data must be a JSON object."));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() !=
        QStringLiteral("lightmediabrowser-tag-data")) {
        return setError(error, QStringLiteral("Unsupported tag data format."));
    }
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != 1.0) {
        return setError(error, QStringLiteral("Unsupported tag data version."));
    }
    const QJsonValue categoriesValue = root.value(QStringLiteral("categories"));
    const QJsonValue videosValue = root.value(QStringLiteral("videos"));
    if (!categoriesValue.isArray() || !videosValue.isArray()) {
        return setError(error, QStringLiteral("Tag data must contain categories and videos arrays."));
    }

    ParsedTagData parsed;
    QSet<QString> categoryNames;
    QSet<QString> definedTags;
    const QJsonArray categories = categoriesValue.toArray();
    for (qsizetype categoryIndex = 0; categoryIndex < categories.size(); ++categoryIndex) {
        if (!categories.at(categoryIndex).isObject()) {
            return setError(error, QStringLiteral("Category %1 must be an object.").arg(categoryIndex));
        }
        const QJsonObject object = categories.at(categoryIndex).toObject();
        const QJsonValue nameValue = object.value(QStringLiteral("name"));
        const QJsonValue tagsValue = object.value(QStringLiteral("tags"));
        const QString name = nameValue.toString().trimmed();
        if (!nameValue.isString() || name.isEmpty() || !tagsValue.isArray()) {
            return setError(error, QStringLiteral("Category %1 has invalid name or tags.").arg(categoryIndex));
        }
        if (categoryNames.contains(name)) {
            return setError(error, QStringLiteral("Duplicate category: %1").arg(name));
        }
        categoryNames.insert(name);

        ImportedCategory category;
        category.name = name;
        QSet<QString> namesInCategory;
        const QJsonArray tags = tagsValue.toArray();
        for (qsizetype tagIndex = 0; tagIndex < tags.size(); ++tagIndex) {
            const QJsonValue tagValue = tags.at(tagIndex);
            const QString tagName = tagValue.toString().trimmed();
            if (!tagValue.isString() || tagName.isEmpty()) {
                return setError(error, QStringLiteral("Tag %1 in category %2 has an invalid name.")
                                          .arg(tagIndex).arg(name));
            }
            if (namesInCategory.contains(tagName)) {
                return setError(error, QStringLiteral("Duplicate tag %1 in category %2.")
                                          .arg(tagName, name));
            }
            namesInCategory.insert(tagName);
            definedTags.insert(tagKey(name, tagName));
            category.tags.append(tagName);
        }
        parsed.categories.append(category);
    }

    QSet<QString> videoPaths;
    const QJsonArray videos = videosValue.toArray();
    for (qsizetype videoIndex = 0; videoIndex < videos.size(); ++videoIndex) {
        if (!videos.at(videoIndex).isObject()) {
            return setError(error, QStringLiteral("Video %1 must be an object.").arg(videoIndex));
        }
        const QJsonObject object = videos.at(videoIndex).toObject();
        const QJsonValue pathValue = object.value(QStringLiteral("path"));
        const QJsonValue fileNameValue = object.value(QStringLiteral("fileName"));
        const QJsonValue fileSizeValue = object.value(QStringLiteral("fileSize"));
        const QJsonValue noteValue = object.value(QStringLiteral("note"));
        const QJsonValue tagsValue = object.value(QStringLiteral("tags"));
        const QString path = pathValue.toString();
        const double fileSize = fileSizeValue.toDouble(-1.0);
        constexpr double maxExactJsonInteger = 9007199254740991.0;
        if (!pathValue.isString() || path.trimmed().isEmpty() ||
            !fileNameValue.isString() || fileNameValue.toString().trimmed().isEmpty() ||
            !fileSizeValue.isDouble() || fileSize < 0.0 ||
            fileSize > maxExactJsonInteger || std::floor(fileSize) != fileSize ||
            !noteValue.isString() || !tagsValue.isArray()) {
            return setError(error, QStringLiteral("Video %1 has invalid fields.").arg(videoIndex));
        }
        if (videoPaths.contains(path)) {
            return setError(error, QStringLiteral("Duplicate video path: %1").arg(path));
        }
        videoPaths.insert(path);

        ImportedVideo video;
        video.path = path;
        video.fileName = fileNameValue.toString();
        video.fileSize = static_cast<qint64>(fileSize);
        video.note = noteValue.toString();
        QSet<QString> assignedTags;
        const QJsonArray tags = tagsValue.toArray();
        for (qsizetype tagIndex = 0; tagIndex < tags.size(); ++tagIndex) {
            if (!tags.at(tagIndex).isObject()) {
                return setError(error, QStringLiteral("Tag assignment %1 for video %2 must be an object.")
                                          .arg(tagIndex).arg(path));
            }
            const QJsonObject tag = tags.at(tagIndex).toObject();
            const QJsonValue categoryValue = tag.value(QStringLiteral("category"));
            const QJsonValue nameValue = tag.value(QStringLiteral("name"));
            const QString category = categoryValue.toString().trimmed();
            const QString name = nameValue.toString().trimmed();
            const QString key = tagKey(category, name);
            if (!categoryValue.isString() || category.isEmpty() ||
                !nameValue.isString() || name.isEmpty() ||
                !definedTags.contains(key)) {
                return setError(error, QStringLiteral("Video %1 refers to an undefined tag.")
                                          .arg(path));
            }
            if (assignedTags.contains(key)) {
                return setError(error, QStringLiteral("Video %1 contains a duplicate tag assignment.")
                                          .arg(path));
            }
            assignedTags.insert(key);
            video.tags.append({category, name});
        }
        parsed.videos.append(video);
    }

    *data = parsed;
    return true;
}
}

Database::Database()
{
    m_database = QSqlDatabase::addDatabase("QSQLITE", "lightmediabrowser");
}

Database::~Database()
{
    const QString connectionName = m_database.connectionName();
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(connectionName);
}

bool Database::open(QString *error, bool *created)
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        if (error) *error = QStringLiteral("管理データフォルダを作成できません: %1").arg(dataDir);
        return false;
    }

    const QString databasePath = dataDir + QStringLiteral("/database.sqlite");
    const bool databaseAlreadyExists = QFileInfo::exists(databasePath);
    m_database.setDatabaseName(databasePath);
    if (!m_database.open()) {
        if (error) *error = m_database.lastError().text();
        return false;
    }

    QSqlQuery pragmas(m_database);
    for (const QString &statement : {
             QStringLiteral("PRAGMA foreign_keys = ON"),
             QStringLiteral("PRAGMA journal_mode = WAL"),
             QStringLiteral("PRAGMA synchronous = NORMAL")}) {
        if (!pragmas.exec(statement)) {
            if (error) *error = QStringLiteral("Could not configure SQLite (%1): %2")
                                    .arg(statement, pragmas.lastError().text());
            return false;
        }
    }
    pragmas.finish();
    if (!migrate(error)) return false;

    const QStringList requiredTables = {
        QStringLiteral("videos"), QStringLiteral("categories"),
        QStringLiteral("tags"), QStringLiteral("video_tags")
    };
    for (const QString &tableName : requiredTables) {
        QSqlQuery tableQuery(m_database);
        tableQuery.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name=:name"));
        tableQuery.bindValue(QStringLiteral(":name"), tableName);
        if (!tableQuery.exec() || !tableQuery.next()) {
            if (error) *error = QStringLiteral("Database schema is incomplete: missing table %1").arg(tableName);
            return false;
        }
    }

    const QStringList requiredVideoColumns = {
        QStringLiteral("file_path"), QStringLiteral("file_name"), QStringLiteral("file_size"),
        QStringLiteral("duration_ms"), QStringLiteral("is_favorite"), QStringLiteral("play_count")
    };
    QSqlQuery columnQuery(m_database);
    if (!columnQuery.exec(QStringLiteral("PRAGMA table_info(videos)"))) {
        if (error) *error = QStringLiteral("Database schema is incomplete: could not inspect videos");
        return false;
    }
    QStringList videoColumns;
    while (columnQuery.next()) videoColumns.append(columnQuery.value(1).toString());
    for (const QString &columnName : requiredVideoColumns) {
        if (!videoColumns.contains(columnName)) {
            if (error) *error = QStringLiteral("Database schema is incomplete: missing videos.%1").arg(columnName);
            return false;
        }
    }

    if (created) *created = !databaseAlreadyExists;
    return true;
}

bool Database::migrate(QString *error)
{
    QSqlQuery query(m_database);
    if (!m_database.transaction()) {
        if (error) *error = QStringLiteral("Could not start migration transaction: %1").arg(m_database.lastError().text());
        return false;
    }

    auto fail = [this, error](const QString &message) {
        m_database.rollback();
        if (error) *error = message;
        return false;
    };
    auto exec = [&query, &fail](const QString &sql) {
        if (query.exec(sql)) return true;
        fail(QStringLiteral("SQL Error (%1): %2").arg(sql, query.lastError().text()));
        return false;
    };

    QSqlQuery versionQuery(m_database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        return fail(QStringLiteral("Could not read database schema version: %1").arg(versionQuery.lastError().text()));
    }
    const int currentVersion = versionQuery.value(0).toInt();
    if (currentVersion > 5) {
        return fail(QStringLiteral("Database schema version %1 is newer than this application supports").arg(currentVersion));
    }

    bool hasTagsTable = false;
    bool hasCategoryColumn = false;
    bool hasCreatedAtColumn = false;
    QSqlQuery tagColumns(m_database);
    if (!tagColumns.exec(QStringLiteral("PRAGMA table_info(tags)"))) {
        return fail(QStringLiteral("Could not inspect tag schema: %1").arg(tagColumns.lastError().text()));
    }
    while (tagColumns.next()) {
        hasTagsTable = true;
        const QString column = tagColumns.value(1).toString();
        hasCategoryColumn = hasCategoryColumn || column == QStringLiteral("category_id");
        hasCreatedAtColumn = hasCreatedAtColumn || column == QStringLiteral("created_at");
    }

    if (hasTagsTable && !hasCategoryColumn) {
        QSqlQuery linkColumns(m_database);
        if (!linkColumns.exec(QStringLiteral("PRAGMA table_info(video_tags)"))) {
            return fail(QStringLiteral("Could not inspect video-tag schema: %1").arg(linkColumns.lastError().text()));
        }
        const bool hasVideoTagsTable = linkColumns.next();
        const QString createdAt = hasCreatedAtColumn ? QStringLiteral("created_at") : QStringLiteral("0");
        if (!exec(QStringLiteral("CREATE TEMP TABLE legacy_tags_backup AS SELECT id, name, %1 AS created_at FROM tags").arg(createdAt)) ||
            !exec(hasVideoTagsTable
                      ? QStringLiteral("CREATE TEMP TABLE legacy_video_tags_backup AS SELECT video_id, tag_id FROM video_tags")
                      : QStringLiteral("CREATE TEMP TABLE legacy_video_tags_backup (video_id INTEGER, tag_id INTEGER)")) ||
            !exec(QStringLiteral("DROP TABLE IF EXISTS video_tags")) ||
            !exec(QStringLiteral("DROP TABLE tags")) ||
            !exec(QStringLiteral("DROP TABLE IF EXISTS categories")) ||
            !exec(QStringLiteral(
                "CREATE TABLE categories (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, created_at INTEGER NOT NULL)")) ||
            !exec(QStringLiteral("INSERT INTO categories (id, name, created_at) VALUES (1, 'Uncategorized', strftime('%s', 'now'))")) ||
            !exec(QStringLiteral(
                "CREATE TABLE tags (id INTEGER PRIMARY KEY AUTOINCREMENT, category_id INTEGER NOT NULL, name TEXT NOT NULL, "
                "created_at INTEGER NOT NULL, FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE CASCADE, "
                "UNIQUE(category_id, name))")) ||
            !exec(QStringLiteral(
                "CREATE TABLE video_tags (video_id INTEGER NOT NULL REFERENCES videos(id) ON DELETE CASCADE, "
                "tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE, PRIMARY KEY(video_id, tag_id))")) ||
            !exec(QStringLiteral(
                "INSERT INTO tags (id, category_id, name, created_at) "
                "SELECT id, 1, name, created_at FROM legacy_tags_backup")) ||
            !exec(QStringLiteral(
                "INSERT INTO video_tags (video_id, tag_id) "
                "SELECT video_id, tag_id FROM legacy_video_tags_backup")) ||
            !exec(QStringLiteral("DROP TABLE legacy_tags_backup")) ||
            !exec(QStringLiteral("DROP TABLE legacy_video_tags_backup"))) {
            return false;
        }
    }

    const QStringList migrations = {
        QStringLiteral(":/migrations/001_initial.sql"),
        QStringLiteral(":/migrations/002_favorites.sql"),
        QStringLiteral(":/migrations/003_play_count.sql"),
        QStringLiteral(":/migrations/004_indexes.sql"),
        QStringLiteral(":/migrations/005_hierarchical_tags.sql")
    };
    for (int i = currentVersion; i < migrations.size(); ++i) {
        QFile migration(migrations.at(i));
        if (!migration.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Migration file could not be opened: %1").arg(migrations.at(i)));
        }
        const QStringList statements = QString::fromUtf8(migration.readAll()).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &statement : statements) {
            const QString sql = statement.trimmed();
            if (sql.isEmpty()) continue;
            if (sql.startsWith(QStringLiteral("ALTER TABLE"), Qt::CaseInsensitive) &&
                sql.contains(QStringLiteral("ADD COLUMN"), Qt::CaseInsensitive)) {
                const QString column = sql.section(QStringLiteral("ADD COLUMN"), 1).trimmed().section(QLatin1Char(' '), 0, 0);
                QSqlQuery columns(m_database);
                if (!columns.exec(QStringLiteral("PRAGMA table_info(videos)"))) {
                    return fail(QStringLiteral("Could not inspect video schema: %1").arg(columns.lastError().text()));
                }
                bool exists = false;
                while (columns.next()) exists = exists || columns.value(1).toString() == column;
                if (exists) continue;
            }
            if (!exec(sql)) return false;
        }
        if (!exec(QStringLiteral("PRAGMA user_version = %1").arg(i + 1))) return false;
    }
    if (!m_database.commit()) {
        return fail(QStringLiteral("Could not commit migration transaction: %1").arg(m_database.lastError().text()));
    }
    return true;
}

bool Database::upsertVideo(const Video &video, QString *error)
{
    return upsertVideos({video}, error);
}

bool Database::upsertVideos(const QList<Video> &videos, QString *error)
{
    if (videos.isEmpty()) return true;
    if (!m_database.transaction()) {
        if (error) *error = QStringLiteral("Could not start video registration transaction: %1")
                                 .arg(m_database.lastError().text());
        return false;
    }

    QSqlQuery query(m_database);
    const QString statement = QStringLiteral(
        "INSERT INTO videos (file_path, file_name, file_size, file_mtime, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(file_path) DO UPDATE SET file_name=excluded.file_name, file_size=excluded.file_size, "
        "file_mtime=excluded.file_mtime, updated_at=excluded.updated_at");
    if (!query.prepare(statement)) {
        const QString message = query.lastError().text();
        m_database.rollback();
        if (error) *error = message;
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const Video &video : videos) {
        query.bindValue(0, video.filePath);
        query.bindValue(1, video.fileName);
        query.bindValue(2, video.fileSize);
        query.bindValue(3, video.fileMtime);
        query.bindValue(4, now);
        query.bindValue(5, now);
        if (query.exec()) continue;

        const QString message = query.lastError().text();
        query.finish();
        m_database.rollback();
        if (error) *error = message;
        return false;
    }

    query.finish();
    if (!m_database.commit()) {
        const QString message = m_database.lastError().text();
        m_database.rollback();
        if (error) *error = QStringLiteral("Could not commit video registration: %1").arg(message);
        return false;
    }
    return true;
}

bool Database::updateVideoMetadata(const Video &video, QString *error)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE videos SET duration_ms=?, width=?, height=?, video_codec=?, audio_codec=?, container_format=?, updated_at=? "
        "WHERE id=?"));
    query.addBindValue(video.durationMs);
    query.addBindValue(video.width);
    query.addBindValue(video.height);
    query.addBindValue(video.videoCodec);
    query.addBindValue(video.audioCodec);
    query.addBindValue(video.containerFormat);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(video.id);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteVideo(const QString &filePath, QString *error)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM videos WHERE file_path=?"));
    query.addBindValue(filePath);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::setFavorite(qint64 videoId, bool favorite, QString *error)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE videos SET is_favorite=?, updated_at=? WHERE id=?"));
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(videoId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::incrementPlayCount(qint64 videoId, QString *error)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE videos SET play_count=play_count+1, updated_at=? WHERE id=?"));
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(videoId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

QList<VideoLightItem> Database::searchVideosLight(const QString &queryText, qint64 tagId,
                                                   bool favoritesOnly, int minPlayCount,
                                                   bool duplicatesOnly, const QString &sortKey,
                                                   int limit, int offset) const
{
    QList<VideoLightItem> result;
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    QString sql = QStringLiteral(
        "WITH filtered AS (SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.is_favorite, v.play_count "
        "FROM videos v WHERE 1=1");
    if (tagId > 0) {
        sql += QStringLiteral(" AND EXISTS (SELECT 1 FROM video_tags vt WHERE vt.video_id=v.id AND vt.tag_id=?)");
    }
    if (hasText) sql += QStringLiteral(" AND (v.file_name LIKE ? OR v.note LIKE ?)");
    if (favoritesOnly) sql += QStringLiteral(" AND v.is_favorite=1");
    if (minPlayCount > 0) sql += QStringLiteral(" AND v.play_count>=?");
    sql += QStringLiteral(")");
    if (duplicatesOnly) {
        sql += QStringLiteral(
            ", duplicate_candidates AS ("
            "SELECT id FROM videos WHERE file_size IN "
            "(SELECT file_size FROM videos GROUP BY file_size HAVING COUNT(*)>1) "
            "UNION "
            "SELECT id FROM videos WHERE file_name IN "
            "(SELECT file_name FROM videos GROUP BY file_name HAVING COUNT(*)>1))");
    }
    sql += QStringLiteral(" SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.is_favorite, v.play_count FROM filtered v WHERE 1=1");
    if (duplicatesOnly) {
        sql += QStringLiteral(" AND v.id IN (SELECT id FROM duplicate_candidates)");
    }
    if (sortKey == QStringLiteral("newest")) sql += QStringLiteral(" ORDER BY v.file_mtime DESC, v.file_name COLLATE NOCASE");
    else if (sortKey == QStringLiteral("size")) sql += QStringLiteral(" ORDER BY v.file_size DESC, v.file_name COLLATE NOCASE");
    else if (sortKey == QStringLiteral("plays")) sql += QStringLiteral(" ORDER BY v.play_count DESC, v.file_name COLLATE NOCASE");
    else sql += QStringLiteral(" ORDER BY v.file_name COLLATE NOCASE, v.file_name");
    if (limit >= 0) sql += QStringLiteral(" LIMIT ? OFFSET ?");
    query.prepare(sql);
    if (tagId > 0) query.addBindValue(tagId);
    if (hasText) {
        const QString pattern = QStringLiteral("%%1%").arg(queryText.trimmed());
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (minPlayCount > 0) query.addBindValue(minPlayCount);
    if (limit >= 0) {
        query.addBindValue(limit);
        query.addBindValue(qMax(0, offset));
    }
    if (!query.exec()) {
        qWarning() << "Video search failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        VideoLightItem item;
        item.id = query.value(0).toLongLong();
        item.filePath = query.value(1).toString();
        item.fileName = query.value(2).toString();
        item.fileSize = query.value(3).toLongLong();
        item.fileMtime = query.value(4).toLongLong();
        item.favorite = query.value(5).toBool();
        item.playCount = query.value(6).toInt();
        result.append(item);
    }
    return result;
}

int Database::countVideosLight(const QString &queryText, qint64 tagId, bool favoritesOnly,
                               int minPlayCount, bool duplicatesOnly) const
{
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    QString sql = QStringLiteral(
        "WITH filtered AS (SELECT v.id, v.file_name, v.file_size FROM videos v WHERE 1=1");
    if (tagId > 0) {
        sql += QStringLiteral(" AND EXISTS (SELECT 1 FROM video_tags vt WHERE vt.video_id=v.id AND vt.tag_id=?)");
    }
    if (hasText) sql += QStringLiteral(" AND (v.file_name LIKE ? OR v.note LIKE ?)");
    if (favoritesOnly) sql += QStringLiteral(" AND v.is_favorite=1");
    if (minPlayCount > 0) sql += QStringLiteral(" AND v.play_count>=?");
    sql += QStringLiteral(")");
    if (duplicatesOnly) {
        sql += QStringLiteral(
            ", duplicate_candidates AS ("
            "SELECT id FROM videos WHERE file_size IN "
            "(SELECT file_size FROM videos GROUP BY file_size HAVING COUNT(*)>1) "
            "UNION "
            "SELECT id FROM videos WHERE file_name IN "
            "(SELECT file_name FROM videos GROUP BY file_name HAVING COUNT(*)>1))");
    }
    sql += QStringLiteral(" SELECT COUNT(*) FROM filtered v WHERE 1=1");
    if (duplicatesOnly) {
        sql += QStringLiteral(" AND v.id IN (SELECT id FROM duplicate_candidates)");
    }
    query.prepare(sql);
    if (tagId > 0) query.addBindValue(tagId);
    if (hasText) {
        const QString pattern = QStringLiteral("%%1%").arg(queryText.trimmed());
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (minPlayCount > 0) query.addBindValue(minPlayCount);
    if (!query.exec() || !query.next()) {
        qWarning() << "Video count query failed:" << query.lastError().text();
        return 0;
    }
    const int count = query.value(0).toInt();
    return count;
}

QList<Video> Database::searchVideos(const QString &queryText, qint64 tagId) const
{
    QList<Video> result;
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    if (tagId <= 0 && !hasText) {
        query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, duration_ms, width, height, video_codec, audio_codec, container_format, rating, note, is_favorite, play_count FROM videos ORDER BY file_name"));
    } else {
        QString sql = QStringLiteral(
            "SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.duration_ms, v.width, v.height, "
            "v.video_codec, v.audio_codec, v.container_format, v.rating, v.note, v.is_favorite, v.play_count FROM videos v WHERE 1=1");
        if (tagId > 0) sql += QStringLiteral(" AND EXISTS (SELECT 1 FROM video_tags vt WHERE vt.video_id=v.id AND vt.tag_id=?)");
        if (hasText) sql += QStringLiteral(" AND (v.file_name LIKE ? OR v.note LIKE ?)");
        sql += QStringLiteral(" ORDER BY v.file_name");
        query.prepare(sql);
        if (tagId > 0) query.addBindValue(tagId);
        if (hasText) {
            const QString pattern = QStringLiteral("%%1%").arg(queryText.trimmed());
            query.addBindValue(pattern);
            query.addBindValue(pattern);
        }
    }
    if (!query.exec()) return result;
    while (query.next()) {
        Video video;
        video.id = query.value(0).toLongLong();
        video.filePath = query.value(1).toString();
        video.fileName = query.value(2).toString();
        video.fileSize = query.value(3).toLongLong();
        video.fileMtime = query.value(4).toLongLong();
        video.durationMs = query.value(5).toLongLong();
        video.width = query.value(6).toInt();
        video.height = query.value(7).toInt();
        video.videoCodec = query.value(8).toString();
        video.audioCodec = query.value(9).toString();
        video.containerFormat = query.value(10).toString();
        video.rating = query.value(11).toInt();
        video.note = query.value(12).toString();
        video.favorite = query.value(13).toBool();
        video.playCount = query.value(14).toInt();
        result.append(video);
    }
    return result;
}

QList<QString> Database::tags() const
{
    QList<QString> result;
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT DISTINCT name FROM tags ORDER BY name"))) {
        while (query.next()) result.append(query.value(0).toString());
    }
    return result;
}

QList<TagInfo> Database::tagsWithCategories() const
{
    QList<TagInfo> result;
    QSqlQuery query(m_database);

    // categories を主体（LEFT JOIN）にし、タグが存在しない場合は t.name が NULL になるように変更
    const QString sql = QStringLiteral(
        "SELECT t.id, t.name, c.name "
        "FROM categories c "
        "LEFT JOIN tags t ON c.id = t.category_id "
        "ORDER BY c.name, t.name"
    );

    if (!query.exec(sql)) return result;

    while (query.next()) {
        TagInfo info;
        info.id = query.value(0).toLongLong();
        info.name = query.value(1).toString();
        info.category = query.value(2).toString();
        result.append(info);
    }
    return result;
}

bool Database::exportTagData(QJsonDocument *document, QString *error) const
{
    if (!document) return setError(error, QStringLiteral("Tag data output is unavailable."));

    QMap<QString, QJsonArray> tagsByCategory;
    QSqlQuery categoryQuery(m_database);
    if (!categoryQuery.exec(QStringLiteral(
            "SELECT c.name, t.name FROM categories c "
            "LEFT JOIN tags t ON t.category_id=c.id ORDER BY c.name, t.name"))) {
        return setError(error, QStringLiteral("Could not read tag categories: %1")
                                   .arg(categoryQuery.lastError().text()));
    }
    while (categoryQuery.next()) {
        const QString category = categoryQuery.value(0).toString();
        if (!tagsByCategory.contains(category)) tagsByCategory.insert(category, {});
        if (!categoryQuery.value(1).isNull()) {
            QJsonArray tags = tagsByCategory.value(category);
            tags.append(categoryQuery.value(1).toString());
            tagsByCategory.insert(category, tags);
        }
    }
    if (categoryQuery.lastError().isValid()) {
        return setError(error, QStringLiteral("Could not read tag categories: %1")
                                   .arg(categoryQuery.lastError().text()));
    }
    categoryQuery.finish();

    QJsonArray categories;
    for (auto it = tagsByCategory.cbegin(); it != tagsByCategory.cend(); ++it) {
        categories.append(QJsonObject{
            {QStringLiteral("name"), it.key()},
            {QStringLiteral("tags"), it.value()}
        });
    }

    QHash<qint64, QJsonArray> tagsByVideo;
    QSqlQuery assignmentQuery(m_database);
    if (!assignmentQuery.exec(QStringLiteral(
            "SELECT vt.video_id, c.name, t.name FROM video_tags vt "
            "JOIN tags t ON t.id=vt.tag_id JOIN categories c ON c.id=t.category_id "
            "ORDER BY c.name, t.name"))) {
        return setError(error, QStringLiteral("Could not read video tag assignments: %1")
                                   .arg(assignmentQuery.lastError().text()));
    }
    while (assignmentQuery.next()) {
        const qint64 videoId = assignmentQuery.value(0).toLongLong();
        QJsonArray tags = tagsByVideo.value(videoId);
        tags.append(QJsonObject{
            {QStringLiteral("category"), assignmentQuery.value(1).toString()},
            {QStringLiteral("name"), assignmentQuery.value(2).toString()}
        });
        tagsByVideo.insert(videoId, tags);
    }
    if (assignmentQuery.lastError().isValid()) {
        return setError(error, QStringLiteral("Could not read video tag assignments: %1")
                                   .arg(assignmentQuery.lastError().text()));
    }
    assignmentQuery.finish();

    QJsonArray videos;
    QSqlQuery videoQuery(m_database);
    if (!videoQuery.exec(QStringLiteral(
            "SELECT id, file_path, file_name, file_size, note FROM videos ORDER BY file_path"))) {
        return setError(error, QStringLiteral("Could not read videos for tag export: %1")
                                   .arg(videoQuery.lastError().text()));
    }
    while (videoQuery.next()) {
        const qint64 videoId = videoQuery.value(0).toLongLong();
        videos.append(QJsonObject{
            {QStringLiteral("path"), videoQuery.value(1).toString()},
            {QStringLiteral("fileName"), videoQuery.value(2).toString()},
            {QStringLiteral("fileSize"), static_cast<double>(videoQuery.value(3).toLongLong())},
            {QStringLiteral("note"), videoQuery.value(4).toString()},
            {QStringLiteral("tags"), tagsByVideo.value(videoId)}
        });
    }
    if (videoQuery.lastError().isValid()) {
        return setError(error, QStringLiteral("Could not read videos for tag export: %1")
                                   .arg(videoQuery.lastError().text()));
    }

    *document = QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("lightmediabrowser-tag-data")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("categories"), categories},
        {QStringLiteral("videos"), videos}
    });
    return true;
}

bool Database::inspectTagDataImport(const QJsonDocument &document, TagDataImportSummary *summary,
                                    QString *error) const
{
    if (!summary) return setError(error, QStringLiteral("Import summary output is unavailable."));
    *summary = {};
    ParsedTagData parsed;
    if (!parseTagData(document, &parsed, error)) return false;

    TagDataImportSummary result;
    QSqlQuery query(m_database);
    if (!query.prepare(QStringLiteral("SELECT 1 FROM videos WHERE file_path=?"))) {
        return setError(error, QStringLiteral("Could not prepare video matching query: %1")
                                   .arg(query.lastError().text()));
    }
    for (const ImportedVideo &video : parsed.videos) {
        query.bindValue(0, video.path);
        if (!query.exec()) {
            return setError(error, QStringLiteral("Could not match video %1: %2")
                                       .arg(video.path, query.lastError().text()));
        }
        const bool found = query.next();
        if (!found && query.lastError().isValid()) {
            return setError(error, QStringLiteral("Could not read match for video %1: %2")
                                       .arg(video.path, query.lastError().text()));
        }
        if (found) ++result.matchedVideos;
        else ++result.unmatchedVideos;
        query.finish();
    }
    *summary = result;
    return true;
}

bool Database::importTagData(const QJsonDocument &document, TagDataImportSummary *summary,
                             QString *error)
{
    if (!summary) return setError(error, QStringLiteral("Import summary output is unavailable."));
    *summary = {};
    ParsedTagData parsed;
    if (!parseTagData(document, &parsed, error)) return false;
    if (!m_database.transaction()) {
        return setError(error, QStringLiteral("Could not start tag data import transaction: %1")
                                   .arg(m_database.lastError().text()));
    }

    auto rollback = [this, error](const QString &message) {
        m_database.rollback();
        if (error) *error = message;
        return false;
    };
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery categoryInsert(m_database);
    if (!categoryInsert.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO categories (name, created_at) VALUES (?, ?)"))) {
        return rollback(QStringLiteral("Could not prepare category import: %1")
                            .arg(categoryInsert.lastError().text()));
    }
    QSqlQuery categoryLookup(m_database);
    if (!categoryLookup.prepare(QStringLiteral("SELECT id FROM categories WHERE name=?"))) {
        return rollback(QStringLiteral("Could not prepare category lookup: %1")
                            .arg(categoryLookup.lastError().text()));
    }
    QSqlQuery tagInsert(m_database);
    if (!tagInsert.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO tags (category_id, name, created_at) VALUES (?, ?, ?)"))) {
        return rollback(QStringLiteral("Could not prepare tag import: %1")
                            .arg(tagInsert.lastError().text()));
    }
    for (const ImportedCategory &category : parsed.categories) {
        categoryInsert.bindValue(0, category.name);
        categoryInsert.bindValue(1, now);
        if (!categoryInsert.exec()) {
            return rollback(QStringLiteral("Could not import category %1: %2")
                                .arg(category.name, categoryInsert.lastError().text()));
        }
        categoryInsert.finish();

        categoryLookup.bindValue(0, category.name);
        if (!categoryLookup.exec()) {
            return rollback(QStringLiteral("Could not find imported category %1: %2")
                                .arg(category.name, categoryLookup.lastError().text()));
        }
        if (!categoryLookup.next()) {
            return rollback(QStringLiteral("Imported category was not found: %1").arg(category.name));
        }
        const qint64 categoryId = categoryLookup.value(0).toLongLong();
        categoryLookup.finish();

        for (const QString &tagName : category.tags) {
            tagInsert.bindValue(0, categoryId);
            tagInsert.bindValue(1, tagName);
            tagInsert.bindValue(2, now);
            if (!tagInsert.exec()) {
                return rollback(QStringLiteral("Could not import tag %1: %2")
                                    .arg(tagName, tagInsert.lastError().text()));
            }
            tagInsert.finish();
        }
    }

    QSqlQuery videoLookup(m_database);
    if (!videoLookup.prepare(QStringLiteral("SELECT id FROM videos WHERE file_path=?"))) {
        return rollback(QStringLiteral("Could not prepare video lookup: %1")
                            .arg(videoLookup.lastError().text()));
    }
    QSqlQuery videoUpdate(m_database);
    if (!videoUpdate.prepare(QStringLiteral("UPDATE videos SET note=?, updated_at=? WHERE id=?"))) {
        return rollback(QStringLiteral("Could not prepare video update: %1")
                            .arg(videoUpdate.lastError().text()));
    }
    QSqlQuery deleteAssignments(m_database);
    if (!deleteAssignments.prepare(QStringLiteral("DELETE FROM video_tags WHERE video_id=?"))) {
        return rollback(QStringLiteral("Could not prepare tag assignment update: %1")
                            .arg(deleteAssignments.lastError().text()));
    }
    QSqlQuery tagLookup(m_database);
    if (!tagLookup.prepare(QStringLiteral(
            "SELECT t.id FROM tags t JOIN categories c ON c.id=t.category_id "
            "WHERE c.name=? AND t.name=?"))) {
        return rollback(QStringLiteral("Could not prepare imported tag lookup: %1")
                            .arg(tagLookup.lastError().text()));
    }
    QSqlQuery assignmentInsert(m_database);
    if (!assignmentInsert.prepare(QStringLiteral(
            "INSERT INTO video_tags (video_id, tag_id) VALUES (?, ?)"))) {
        return rollback(QStringLiteral("Could not prepare tag assignment import: %1")
                            .arg(assignmentInsert.lastError().text()));
    }

    TagDataImportSummary result;
    for (const ImportedVideo &video : parsed.videos) {
        videoLookup.bindValue(0, video.path);
        if (!videoLookup.exec()) {
            return rollback(QStringLiteral("Could not match video %1: %2")
                                .arg(video.path, videoLookup.lastError().text()));
        }
        const bool videoFound = videoLookup.next();
        if (!videoFound && videoLookup.lastError().isValid()) {
            return rollback(QStringLiteral("Could not read match for video %1: %2")
                                .arg(video.path, videoLookup.lastError().text()));
        }
        if (!videoFound) {
            ++result.unmatchedVideos;
            videoLookup.finish();
            continue;
        }
        const qint64 videoId = videoLookup.value(0).toLongLong();
        videoLookup.finish();
        ++result.matchedVideos;

        videoUpdate.bindValue(0, video.note);
        videoUpdate.bindValue(1, now);
        videoUpdate.bindValue(2, videoId);
        if (!videoUpdate.exec()) {
            return rollback(QStringLiteral("Could not update video %1: %2")
                                .arg(video.path, videoUpdate.lastError().text()));
        }
        videoUpdate.finish();

        deleteAssignments.bindValue(0, videoId);
        if (!deleteAssignments.exec()) {
            return rollback(QStringLiteral("Could not replace tags for video %1: %2")
                                .arg(video.path, deleteAssignments.lastError().text()));
        }
        deleteAssignments.finish();

        for (const ImportedTag &tag : video.tags) {
            tagLookup.bindValue(0, tag.category);
            tagLookup.bindValue(1, tag.name);
            if (!tagLookup.exec()) {
                return rollback(QStringLiteral("Could not find imported tag %1/%2: %3")
                                    .arg(tag.category, tag.name, tagLookup.lastError().text()));
            }
            if (!tagLookup.next()) {
                return rollback(QStringLiteral("Imported tag was not found: %1/%2")
                                    .arg(tag.category, tag.name));
            }
            const qint64 tagId = tagLookup.value(0).toLongLong();
            tagLookup.finish();

            assignmentInsert.bindValue(0, videoId);
            assignmentInsert.bindValue(1, tagId);
            if (!assignmentInsert.exec()) {
                return rollback(QStringLiteral("Could not assign tag %1/%2 to video %3: %4")
                                    .arg(tag.category, tag.name, video.path,
                                         assignmentInsert.lastError().text()));
            }
            assignmentInsert.finish();
        }
    }

    if (!m_database.commit()) {
        const QString commitError = m_database.lastError().text();
        m_database.rollback();
        return setError(error, QStringLiteral("Could not commit tag data import: %1").arg(commitError));
    }
    *summary = result;
    return true;
}

Video Database::findById(qint64 id) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, duration_ms, width, height, video_codec, audio_codec, container_format, rating, note, is_favorite, play_count FROM videos WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) return {};
    Video video;
    video.id = query.value(0).toLongLong();
    video.filePath = query.value(1).toString();
    video.fileName = query.value(2).toString();
    video.fileSize = query.value(3).toLongLong();
    video.fileMtime = query.value(4).toLongLong();
    video.durationMs = query.value(5).toLongLong();
    video.width = query.value(6).toInt();
    video.height = query.value(7).toInt();
    video.videoCodec = query.value(8).toString();
    video.audioCodec = query.value(9).toString();
    video.containerFormat = query.value(10).toString();
    video.rating = query.value(11).toInt();
    video.note = query.value(12).toString();
    video.favorite = query.value(13).toBool();
    video.playCount = query.value(14).toInt();
    return video;
}

QList<QString> Database::tagsForVideo(qint64 videoId) const
{
    QList<QString> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT t.name FROM tags t JOIN video_tags vt ON vt.tag_id=t.id WHERE vt.video_id=? ORDER BY t.name"));
    query.addBindValue(videoId);
    if (!query.exec()) return result;
    while (query.next()) result.append(query.value(0).toString());
    return result;
}

QList<qint64> Database::tagIdsForVideo(qint64 videoId) const
{
    QList<qint64> result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT tag_id FROM video_tags WHERE video_id=? ORDER BY tag_id"));
    query.addBindValue(videoId);
    if (!query.exec()) return result;
    while (query.next()) result.append(query.value(0).toLongLong());
    return result;
}

bool Database::addCategory(const QString &categoryName, QString *error)
{
    if (categoryName.trimmed().isEmpty()) return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO categories (name, created_at) VALUES (?, ?)"));
    query.addBindValue(categoryName.trimmed());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteCategory(const QString &categoryName, QString *error)
{
    if (categoryName.trimmed().isEmpty()) return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM categories WHERE name = ?"));
    query.addBindValue(categoryName.trimmed());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::addTag(const QString &categoryName, const QString &tagName, QString *error)
{
    if (categoryName.trimmed().isEmpty() || tagName.trimmed().isEmpty()) return false;

    // まずカテゴリが存在するか確認し、なければ作成する
    if (!addCategory(categoryName, error)) return false;

    QSqlQuery catQuery(m_database);
    catQuery.prepare(QStringLiteral("SELECT id FROM categories WHERE name = ?"));
    catQuery.addBindValue(categoryName.trimmed());
    if (!catQuery.exec() || !catQuery.next()) {
        if (error) *error = QStringLiteral("Category not found: %1").arg(categoryName);
        return false;
    }
    const qint64 categoryId = catQuery.value(0).toLongLong();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO tags (category_id, name, created_at) VALUES (?, ?, ?)"));
    query.addBindValue(categoryId);
    query.addBindValue(tagName.trimmed());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteTag(qint64 tagId, QString *error)
{
    if (tagId <= 0) return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM tags WHERE id = ?"));
    query.addBindValue(tagId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::setVideoTags(qint64 videoId, const QList<qint64> &tagIds, QString *error)
{
    QSqlQuery query(m_database);
    // トランザクション開始
    if (!m_database.transaction()) {
        if (error) *error = QStringLiteral("Could not start transaction: %1").arg(m_database.lastError().text());
        return false;
    }

    // 既存の紐付けを一旦全削除
    query.prepare(QStringLiteral("DELETE FROM video_tags WHERE video_id = ?"));
    query.addBindValue(videoId);
    if (!query.exec()) {
        m_database.rollback();
        if (error) *error = query.lastError().text();
        return false;
    }

    QSqlQuery linkQuery(m_database);
    linkQuery.prepare(QStringLiteral("INSERT OR IGNORE INTO video_tags (video_id, tag_id) VALUES (?, ?)"));
    for (qint64 tagId : tagIds) {
        if (tagId <= 0) continue;
        linkQuery.bindValue(0, videoId);
        linkQuery.bindValue(1, tagId);
        if (!linkQuery.exec()) {
            m_database.rollback();
            if (error) *error = linkQuery.lastError().text();
            return false;
        }
        linkQuery.finish();
    }

    if (!m_database.commit()) {
        const QString commitError = m_database.lastError().text();
        m_database.rollback();
        if (error) *error = QStringLiteral("Could not commit transaction: %1").arg(commitError);
        return false;
    }
    return true;
}

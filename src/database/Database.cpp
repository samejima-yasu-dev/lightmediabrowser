#include "database/Database.h"

#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDateTime>
#include <QStringList>

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
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO videos (file_path, file_name, file_size, file_mtime, created_at, updated_at) "
                                 "VALUES (?, ?, ?, ?, ?, ?) "
                                 "ON CONFLICT(file_path) DO UPDATE SET file_name=excluded.file_name, file_size=excluded.file_size, "
                                 "file_mtime=excluded.file_mtime, updated_at=excluded.updated_at"));
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    query.addBindValue(video.filePath);
    query.addBindValue(video.fileName);
    query.addBindValue(video.fileSize);
    query.addBindValue(video.fileMtime);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
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
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
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
            ", duplicate_sizes AS (SELECT file_size FROM videos GROUP BY file_size HAVING COUNT(*)>1)");
    }
    sql += QStringLiteral(" SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.is_favorite, v.play_count FROM filtered v WHERE 1=1");
    if (duplicatesOnly) {
        sql += QStringLiteral(" AND v.file_size IN (SELECT file_size FROM duplicate_sizes)");
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
    qInfo() << "Video page query:" << result.size() << "rows in"
            << elapsedTimer.elapsed() << "ms; offset:" << offset;
    return result;
}

int Database::countVideosLight(const QString &queryText, qint64 tagId, bool favoritesOnly,
                               int minPlayCount, bool duplicatesOnly) const
{
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    QString sql = QStringLiteral(
        "WITH filtered AS (SELECT v.file_name, v.file_size FROM videos v WHERE 1=1");
    if (tagId > 0) {
        sql += QStringLiteral(" AND EXISTS (SELECT 1 FROM video_tags vt WHERE vt.video_id=v.id AND vt.tag_id=?)");
    }
    if (hasText) sql += QStringLiteral(" AND (v.file_name LIKE ? OR v.note LIKE ?)");
    if (favoritesOnly) sql += QStringLiteral(" AND v.is_favorite=1");
    if (minPlayCount > 0) sql += QStringLiteral(" AND v.play_count>=?");
    sql += QStringLiteral(")");
    if (duplicatesOnly) {
        sql += QStringLiteral(
            ", duplicate_sizes AS (SELECT file_size FROM videos GROUP BY file_size HAVING COUNT(*)>1)");
    }
    sql += QStringLiteral(" SELECT COUNT(*) FROM filtered v WHERE 1=1");
    if (duplicatesOnly) {
        sql += QStringLiteral(" AND v.file_size IN (SELECT file_size FROM duplicate_sizes)");
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
    qInfo() << "Video count query:" << count << "rows in"
            << elapsedTimer.elapsed() << "ms";
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

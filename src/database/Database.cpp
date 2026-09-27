#include "database/Database.h"

#include <QDir>
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
    pragmas.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragmas.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragmas.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    pragmas.finish();
    if (!migrate(error)) return false;

    if (created) *created = !databaseAlreadyExists;
    return true;
}

bool Database::migrate(QString *error)
{
    QSqlQuery query(m_database);
    
    // 既存のデータベースファイル（または初期マイグレーション001で作成された古いtagsテーブル）に
    // category_idカラムが存在しない場合、安全に旧タグ・紐付けテーブルを破棄して新しい階層型タグ構造を作成します。
    QSqlQuery checkTags(m_database);
    if (checkTags.exec(QStringLiteral("PRAGMA table_info(tags)"))) {
        bool hasCategoryColumn = false;
        while (checkTags.next()) {
            if (checkTags.value(1).toString() == QStringLiteral("category_id")) {
                hasCategoryColumn = true;
                break;
            }
        }
        // 古いtagsテーブル（category_idがない）が存在する場合は、古いテーブルを削除してスキーマを刷新
        if (checkTags.isActive() && !hasCategoryColumn) {
            query.exec(QStringLiteral("DROP TABLE IF EXISTS video_tags"));
            query.exec(QStringLiteral("DROP TABLE IF EXISTS tags"));
            query.exec(QStringLiteral("DROP TABLE IF EXISTS categories"));
        }
    }

    for (const QString &resourcePath : {
        QStringLiteral(":/migrations/001_initial.sql"),
        QStringLiteral(":/migrations/002_favorites.sql"),
        QStringLiteral(":/migrations/003_play_count.sql"),
        QStringLiteral(":/migrations/004_indexes.sql"),
        QStringLiteral(":/migrations/005_hierarchical_tags.sql")
    }) {
        QFile migration(resourcePath);
        if (!migration.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) *error = QStringLiteral("Migration file could not be opened: %1").arg(resourcePath);
            return false;
        }
        const QStringList statements = QString::fromUtf8(migration.readAll()).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &statement : statements) {
            const QString sql = statement.trimmed();
            if (sql.isEmpty()) continue;
            if (sql.startsWith(QStringLiteral("ALTER TABLE")) && sql.contains(QStringLiteral("ADD COLUMN"))) {
                const QString column = sql.section(QStringLiteral("ADD COLUMN"), 1).trimmed().section(QLatin1Char(' '), 0, 0);
                QSqlQuery columns(m_database);
                columns.exec(QStringLiteral("PRAGMA table_info(videos)"));
                bool exists = false;
                while (columns.next()) exists = exists || columns.value(1).toString() == column;
                if (exists) continue;
            }
            if (!query.exec(sql)) {
                const QString errText = query.lastError().text();
                // すでに存在するテーブル/インデックスや、初期データのINSERT OR IGNORE以外のエラーをキャッチ
                if (!errText.contains(QStringLiteral("already exists"), Qt::CaseInsensitive) &&
                    !errText.contains(QStringLiteral("UNIQUE constraint failed"), Qt::CaseInsensitive)) {
                    if (error) *error = QStringLiteral("SQL Error (%1): %2").arg(sql, errText);
                    return false;
                }
            }
            query.finish();
        }
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

QList<VideoLightItem> Database::searchVideosLight(const QString &queryText, const QString &tag) const
{
    QList<VideoLightItem> result;
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    const bool hasTag = !tag.trimmed().isEmpty();
    if (!hasText && !hasTag) {
        query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, is_favorite, play_count FROM videos ORDER BY file_name"));
    } else if (hasTag) {
        query.prepare(QStringLiteral("SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.is_favorite, v.play_count FROM videos v JOIN video_tags vt ON vt.video_id = v.id JOIN tags t ON t.id = vt.tag_id WHERE t.name = ? AND (? = '' OR v.file_name LIKE ? OR v.file_path LIKE ? OR v.note LIKE ?) ORDER BY v.file_name"));
        const QString pattern = QStringLiteral("%%1%").arg(queryText);
        query.addBindValue(tag);
        query.addBindValue(queryText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    } else {
        query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, is_favorite, play_count FROM videos WHERE file_name LIKE ? OR file_path LIKE ? OR note LIKE ? ORDER BY file_name"));
        const QString pattern = QStringLiteral("%%1%").arg(queryText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (!query.exec()) return result;
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

QList<Video> Database::searchVideos(const QString &queryText, const QString &tag) const
{
    QList<Video> result;
    QSqlQuery query(m_database);
    const bool hasText = !queryText.trimmed().isEmpty();
    const bool hasTag = !tag.trimmed().isEmpty();
    if (!hasText && !hasTag) {
        query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, duration_ms, width, height, video_codec, audio_codec, container_format, rating, note, is_favorite, play_count FROM videos ORDER BY file_name"));
    } else if (hasTag) {
        query.prepare(QStringLiteral("SELECT v.id, v.file_path, v.file_name, v.file_size, v.file_mtime, v.duration_ms, v.width, v.height, v.video_codec, v.audio_codec, v.container_format, v.rating, v.note, v.is_favorite, v.play_count FROM videos v JOIN video_tags vt ON vt.video_id = v.id JOIN tags t ON t.id = vt.tag_id WHERE t.name = ? AND (? = '' OR v.file_name LIKE ? OR v.file_path LIKE ? OR v.note LIKE ?) ORDER BY v.file_name"));
        const QString pattern = QStringLiteral("%%1%").arg(queryText);
        query.addBindValue(tag);
        query.addBindValue(queryText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    } else {
        query.prepare(QStringLiteral("SELECT id, file_path, file_name, file_size, file_mtime, duration_ms, width, height, video_codec, audio_codec, container_format, rating, note, is_favorite, play_count FROM videos WHERE file_name LIKE ? OR file_path LIKE ? OR note LIKE ? ORDER BY file_name"));
        const QString pattern = QStringLiteral("%%1%").arg(queryText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
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
        "SELECT t.name, c.name "
        "FROM categories c "
        "LEFT JOIN tags t ON c.id = t.category_id "
        "ORDER BY c.name, t.name"
    );

    if (!query.exec(sql)) return result;

    while (query.next()) {
        QString tagName = query.value(0).toString(); // タグが無い場合は空文字 (またはisNull)
        QString categoryName = query.value(1).toString();
        
        result.append({tagName, categoryName});
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

bool Database::deleteTag(const QString &tagName, QString *error)
{
    if (tagName.trimmed().isEmpty()) return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM tags WHERE name = ?"));
    query.addBindValue(tagName.trimmed());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::setVideoTags(qint64 videoId, const QStringList &tagNames, QString *error)
{
    QSqlQuery query(m_database);
    // トランザクション開始
    if (!m_database.transaction()) {
        if (error) *error = QStringLiteral("Could not start transaction");
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

    // 新しいタグごとにIDを引いて紐付けを挿入
    for (const QString &tagName : tagNames) {
        if (tagName.trimmed().isEmpty()) continue;
        
        QSqlQuery tagQuery(m_database);
        tagQuery.prepare(QStringLiteral("SELECT id FROM tags WHERE name = ?"));
        tagQuery.addBindValue(tagName.trimmed());
        if (tagQuery.exec() && tagQuery.next()) {
            const qint64 tagId = tagQuery.value(0).toLongLong();
            QSqlQuery linkQuery(m_database);
            linkQuery.prepare(QStringLiteral("INSERT OR IGNORE INTO video_tags (video_id, tag_id) VALUES (?, ?)"));
            linkQuery.addBindValue(videoId);
            linkQuery.addBindValue(tagId);
            if (!linkQuery.exec()) {
                m_database.rollback();
                if (error) *error = linkQuery.lastError().text();
                return false;
            }
        }
    }

    if (!m_database.commit()) {
        if (error) *error = QStringLiteral("Could not commit transaction");
        return false;
    }
    return true;
}

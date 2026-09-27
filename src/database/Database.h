#pragma once

#include "domain/Video.h"

#include <QList>
#include <QString>
#include <QSqlDatabase>

struct MetadataResult;

/**
 * @brief タグ名とそのカテゴリを保持する構造体
 */
struct TagInfo
{
    QString name;       ///< タグの表示名
    QString category;   ///< 大項目（カテゴリ）名
};

/**
 * @brief 動画一覧表示用に必要な最小限のフィールドを持つ軽量構造体（メモリ消費削減・描画高速化用）
 */
struct VideoLightItem
{
    qint64 id = 0;              ///< 主キーID
    QString filePath;           ///< ファイルパス
    QString fileName;           ///< ファイル名
    qint64 fileSize = 0;        ///< ファイルサイズ
    qint64 fileMtime = 0;       ///< 最終更新日時
    bool favorite = false;      ///< お気に入り状態
    int playCount = 0;          ///< 再生回数
};

/**
 * @brief SQLiteデータベースとの接続、クエリ実行、マイグレーションを管理するクラス
 */
class Database
{
public:
    Database();
    ~Database();

    /**
     * @brief データベース接続を開き、必要に応じて自動マイグレーションを実行する
     * @param error エラーメッセージ格納用ポインタ
     * @param created 成功時に新規データベースを作成したかを格納するポインタ
     * @return 成功した場合はtrue、失敗した場合はfalse
     */
    bool open(QString *error = nullptr, bool *created = nullptr);

    /**
     * @brief キーワードやタグに一致する動画の詳細情報リストを検索する
     * @param query 検索キーワード（ファイル名、パス、メモ等）
     * @param tag 絞り込み対象のタグ名
     * @return 条件に一致するVideoオブジェクトのリスト
     */
    QList<Video> searchVideos(const QString &query = {}, const QString &tag = {}) const;

    /**
     * @brief 一覧表示用の軽量な動画データリストを検索する（高速・低メモリ）
     * @param query 検索キーワード
     * @param tag 絞り込み対象のタグ名
     * @return VideoLightItemオブジェクトのリスト
     */
    QList<VideoLightItem> searchVideosLight(const QString &query = {}, const QString &tag = {}) const;

    /**
     * @brief 指定したIDの動画詳細情報を取得する
     * @param id 動画の主キーID
     * @return Videoオブジェクト（見つからない場合は空）
     */
    Video findById(qint64 id) const;

    /**
     * @brief 指定した動画に関連付けられたすべてのタグ名を取得する
     * @param videoId 動画の主キーID
     * @return タグ名のリスト
     */
    QList<QString> tagsForVideo(qint64 videoId) const;

    /**
     * @brief 登録されているすべてのタグ名を取得する
     * @return タグ名のリスト
     */
    QList<QString> tags() const;

    /**
     * @brief カテゴリ情報付きのタグ一覧を取得する
     * @return TagInfoオブジェクトのリスト
     */
    QList<TagInfo> tagsWithCategories() const;

    /**
     * @brief 新しい大項目（カテゴリ）を追加する
     * @param categoryName カテゴリ名
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool addCategory(const QString &categoryName, QString *error = nullptr);

    /**
     * @brief 大項目（カテゴリ）を削除する（紐づく小項目もカスケード削除）
     * @param categoryName カテゴリ名
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool deleteCategory(const QString &categoryName, QString *error = nullptr);

    /**
     * @brief 指定した大項目に新しい小項目（タグ）を追加する
     * @param categoryName カテゴリ名
     * @param tagName タグ名
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool addTag(const QString &categoryName, const QString &tagName, QString *error = nullptr);

    /**
     * @brief 小項目（タグ）を削除する
     * @param tagName タグ名
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool deleteTag(const QString &tagName, QString *error = nullptr);

    /**
     * @brief 動画にタグを割り当てる（既存の割り当てを上書きまたは再設定）
     * @param videoId 動画ID
     * @param tagNames 割り当てるタグ名のリスト
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool setVideoTags(qint64 videoId, const QStringList &tagNames, QString *error = nullptr);

    /**
     * @brief 動画情報をデータベースに挿入または更新（Upsert）する
     * @param video 登録する動画データ
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool upsertVideo(const Video &video, QString *error = nullptr);

    /**
     * @brief 動画のお気に入り状態を更新する
     * @param videoId 動画ID
     * @param favorite お気に入りフラグ
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool setFavorite(qint64 videoId, bool favorite, QString *error = nullptr);

    /**
     * @brief 動画の再生回数を1インクリメントする
     * @param videoId 動画ID
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool incrementPlayCount(qint64 videoId, QString *error = nullptr);

private:
    /**
     * @brief データベースのスキーママイグレーションスクリプトを実行する
     * @param error エラーメッセージ格納用ポインタ
     * @return 成否
     */
    bool migrate(QString *error);

    QSqlDatabase m_database; ///< QtのSQLデータベース接続インスタンス
};

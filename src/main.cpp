#include "ui/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

/**
 * @brief アプリケーションのエントリーポイント
 * @param argc コマンドライン引数の数
 * @param argv コマンドライン引数の配列
 * @return 終了ステータスコード
 */
int main(int argc, char *argv[])
{
    // リソースファイル（qrc）を初期化
    Q_INIT_RESOURCE(resources);
    
    // Qtアプリケーションインスタンスを生成
    QApplication application(argc, argv);
    
    // クロスプラットフォームで統一されたモダンなルック＆フィール（Fusion）を適用
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    
    // アプリケーション名と組織名を設定（設定の保存やデータ保存パスで使用）
    application.setApplicationName(QStringLiteral("LightMediaBrowser"));
    application.setOrganizationName(QStringLiteral("LightMediaBrowser"));
    
    // メインウィンドウをインスタンス化して表示
    MainWindow window;
    window.show();
    
    // イベントループを開始し、アプリ終了ステータスを返す
    return application.exec();
}

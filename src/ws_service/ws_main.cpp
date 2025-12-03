#include <QCoreApplication>
#include <ws/ws_hub.hpp>
#include "../db_core/SeatDatabase.h"

int main(int argc, char* argv[]){
    QCoreApplication app(argc, argv);
    // 打开数据库（使用你代码中的默认路径与建表逻辑）
    auto& db = SeatDatabase::getInstance();  // 单例默认路径见你的头文件
    db.initialize();

    WsHub hub;
    if (!hub.start(12345, QHostAddress::LocalHost)) return 1;
    return app.exec();
}

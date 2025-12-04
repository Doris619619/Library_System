
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QTimer>
#include <QHostAddress>
#include <QSet>
#include <QHash>


#include <ws/ws_hub.hpp>
// 前置：数据库单例
#include "../db_core/SeatDatabase.h"


#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>


#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>



#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QTimer>
#include <QHostAddress>
#include <QSet>
#include <QHash>


#include <ws/ws_hub.hpp>
// 前置：数据库单例
#include "../db_core/SeatDatabase.h"


#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>


#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>


//Qt WebSocket 服务端的构造函数，实现了一个周期性推送数据的功能

WsHub::WsHub(QObject* parent)
    : QObject(parent),
    server_(QStringLiteral("SeatUI-WS"), QWebSocketServer::NonSecureMode, this)
{
    tick_.setInterval(2000); // 2s 定时推送
    connect(&tick_, &QTimer::timeout, this, &WsHub::onTickPush);
}

bool WsHub::start(quint16 port, const QHostAddress& host) {
    if (!server_.listen(host, port)) {
        qWarning() << "[WS] Hub start failed on" << host.toString() << ":" << port;
        return false;
    }
    connect(&server_, &QWebSocketServer::newConnection, this, &WsHub::onNewConnection);
    tick_.start();  // 启动定时器
    return true;
}

void WsHub::onNewConnection() {
    auto* socket = server_.nextPendingConnection();
    if (!socket) return;

    clients_ << socket;

    // 处理连接后的消息
    connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString& message) {
        onSocketText(socket, message);
    });

    // 处理断开连接
    connect(socket, &QWebSocket::disconnected, this, [this, socket] {
        clients_.remove(socket);
        socket->deleteLater();
    });

    
}


/*
void WsHub::onSocketText(QWebSocket* socket, const QString& message) {
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(message.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject()) return;

    const auto obj = document.object();
    const auto type = obj.value("type").toString();

    if (type == "hello") {
        const auto role = obj.value("role").toString();
        roles_[socket] = role;  // 保存连接角色
    }
}

*/

//实现学生端和admin交互
void WsHub::onSocketText(QWebSocket* socket, const QString& message) {
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(message.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "[WS] Invalid JSON received:" << error.errorString();
        return;
    }

    const auto obj = document.object();
    const auto type = obj.value("type").toString();

    if (type == "hello") {
        const auto role = obj.value("role").toString();
        roles_[socket] = role;  // 保存连接角色

        qDebug() << "[WS] Client connected with role:" << role;

        // 回复确认
        QJsonObject reply;
        reply["type"] = "hello_ack";
        reply["role"] = role;
        reply["status"] = "ok";
        socket->sendTextMessage(QJsonDocument(reply).toJson());
    }
    else if (type == "student_help") {
        // ✅ 转发学生求助消息给管理员端
        qDebug() << "[WS] Forwarding student_help to admin clients";

        bool forwarded = false;
        for (auto* client : clients_) {
            if (roles_.value(client) == "admin") {
                client->sendTextMessage(message);
                forwarded = true;
            }
        }

        if (forwarded) {
            qDebug() << "[WS] student_help forwarded to admin(s)";

            // 可选：给学生端一个确认
            QJsonObject ack;
            ack["type"] = "help_ack";
            ack["status"] = "sent_to_admin";
            ack["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            socket->sendTextMessage(QJsonDocument(ack).toJson());
        } else {
            qWarning() << "[WS] No admin connected to forward student_help";
        }
    }
    else {
        qDebug() << "[WS] Received unknown message type:" << type;
    }
}

//这个是原先没有修改过的接收字段
/*
void WsHub::onTickPush() {
    // 获取当前座位状态
    auto seatStatusList = SeatDatabase::getInstance().getCurrentSeatStatus();

    // 构造 seat_snapshot 消息
    QJsonObject root;
    root["type"] = "seat_snapshot";
    root["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonArray items;
    for (const auto& seatStatus : seatStatusList) {
        QJsonObject seatObj;
        seatObj["seat_id"] = QString::fromStdString(seatStatus.seat_id);
        seatObj["state"] = QString::fromStdString(seatStatus.state);
        seatObj["since"] = QString::fromStdString(seatStatus.last_update);
        items.append(seatObj);
    }

    root["items"] = items;

    // 广播给所有连接的客户端
    broadcast(QJsonDocument(root).toJson());
}
*/







void WsHub::onTickPush() {
    // 1. 获取座位状态和统计信息
    //auto seatStatusList = SeatDatabase::getInstance().getCurrentSeatStatus();
    //auto basicStats = SeatDatabase::getInstance().getCurrentBasicStats();

    auto& db = SeatDatabase::getInstance("D:/CSC3002/Library_System/out/seating_system.db");
    auto seatStatusList = db.getCurrentSeatStatus();
    auto basicStats     = db.getCurrentBasicStats();
        // ……其余逻辑不变


    //用来debug
    qDebug() << "[WS] tick-raw:"
             << "seats=" << seatStatusList.size()
             << "total=" << basicStats.total_seats
             << "occ=" << basicStats.occupied_seats
             << "anom=" << basicStats.anomaly_seats
             << "rate=" << basicStats.overall_occupancy_rate;

    // 2. 构造消息 - 改为 "seat_update"
    QJsonObject root;
    root["type"] = "seat_update";  // 改为 seat_update
    root["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // 3. 添加统计信息
    QJsonObject statsObj;
    statsObj["total_seats"] = basicStats.total_seats;
    statsObj["occupied_seats"] = basicStats.occupied_seats;
    statsObj["anomaly_seats"] = basicStats.anomaly_seats;
    statsObj["overall_occupancy_rate"] = basicStats.overall_occupancy_rate;
    root["stats"] = statsObj;

    // 4. 构建座位数组 - 字段名改为 "seats"
    // QJsonArray seatsArray;  // 变量名改为 seatsArray
    // for (const auto& seatStatus : seatStatusList) {
    //     QJsonObject seatObj;
    //     seatObj["id"] = QString::fromStdString(seatStatus.seat_id);  // 改为 "id"
    //     seatObj["state"] = QString::fromStdString(seatStatus.state);
    //     seatObj["last_update"] = QString::fromStdString(seatStatus.last_update);  // 改为 "last_update"
    //     seatsArray.append(seatObj);
    // }
    // 4. 构建座位数组 - 字段名 "seats"
    QJsonArray seatsArray;
    for (const auto& seatStatus : seatStatusList) {
        QJsonObject seatObj;
        seatObj["id"] = QString::fromStdString(seatStatus.seat_id);

        // 状态原样转文字（DB是 "Seated"/"Unseated"/"Anomaly"）
        seatObj["state"] = QString::fromStdString(seatStatus.state);

        // 关键：把 DB 的 "yyyy-MM-dd HH:mm:ss"(本地时) 统一转为 UTC ISO 再发
        QString last = QString::fromStdString(seatStatus.last_update);  // e.g. "2025-12-03 16:42:37"
        QString lastIso = last;
        QDateTime dt = QDateTime::fromString(last, "yyyy-MM-dd HH:mm:ss");
        if (dt.isValid()) {
            // 注意这里转 UTC，生成 "...Z"
            lastIso = dt.toUTC().toString(Qt::ISODate);
        }
        seatObj["last_update"] = lastIso;

        seatsArray.append(seatObj);
    }
    root["seats"] = seatsArray;

    // 包头仍用 UTC ISO（保持你现在的写法）
    root["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);



    // 5. 广播
    broadcast(QJsonDocument(root).toJson());
}





void WsHub::broadcast(const QString& message, const QString& onlyRole) {
    for (auto* client : clients_) {
        if (!onlyRole.isEmpty()) {
            if (roles_.value(client) != onlyRole) continue;
        }
        client->sendTextMessage(message);
    }
}

QString WsHub::makeSeatSnapshotJson() {
    // 返回座位快照的 JSON
    auto seatStatusList = SeatDatabase::getInstance().getCurrentSeatStatus();

    QJsonObject root;
    root["type"] = "seat_snapshot";
    root["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonArray items;
    for (const auto& seatStatus : seatStatusList) {
        QJsonObject seatObj;
        seatObj["seat_id"] = QString::fromStdString(seatStatus.seat_id);
        seatObj["state"] = QString::fromStdString(seatStatus.state);
        seatObj["since"] = QString::fromStdString(seatStatus.last_update);
        items.append(seatObj);
    }

    root["items"] = items;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}




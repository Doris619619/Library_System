#pragma once
#include <QObject>
#include <QTimer>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QSet>


#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>



#include <QByteArray>






#include <QTimer>
#include <QHostAddress>

#include <QHash>

// 前置：数据库单例
#include "../src/db_core/SeatDatabase.h"   // 按你的实际头文件路径

class WsHub : public QObject {
    Q_OBJECT
public:
    explicit WsHub(QObject* parent=nullptr);
    bool start(quint16 port, const QHostAddress& host = QHostAddress::LocalHost);

signals:
    void started();

private slots:
    void onNewConnection();
    void onSocketText(QWebSocket* sock, const QString& text);
    //void onSocketClosed(QWebSocket* sock);
    void onTickPush();

private:
    void broadcast(const QString& msg, const QString& onlyRole = QString());
    QString makeSeatSnapshotJson();

    QWebSocketServer server_;
    QSet<QWebSocket*> clients_;
    QHash<QWebSocket*, QString> roles_; // socket -> "admin" / "student" / ""
    QTimer tick_;
};

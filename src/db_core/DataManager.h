#pragma once
#include "SeatDatabase.h"
#include "DataTypes.h"
#include <QObject>
#include <QTimer>

class DataManager : public QObject {
    Q_OBJECT
public:
    static DataManager* instance();
    
    // 数据库连接
    bool connectToDatabase(const QString& dbPath = "../output/seating_system.db");
    
    // 数据获取方法（供UI调用）
    Q_INVOKABLE QVariantList getCurrentSeatStatus();
    Q_INVOKABLE QVariantMap getCurrentBasicStats();
    Q_INVOKABLE QVariantList getTodayHourlyData();
    
    // 数据更新方法
    bool insertSeatEvent(const QString& seatId, const QString& state, 
                        const QString& timestamp, int durationSec = 0);
    
signals:
    // 数据更新信号（供UI绑定）
    void seatStatusUpdated(const QVariantList& seats);
    void basicStatsUpdated(const QVariantMap& stats);
    void hourlyDataUpdated(const QVariantList& hourlyData);
    
private slots:
    void onRefreshTimer();

private:
    DataManager(QObject* parent = nullptr);
    ~DataManager();
    
    SeatDatabase& db_;
    QTimer* refreshTimer_;
    static DataManager* instance_;
};

public slots:
    void broadcastDataUpdates();

private:
    QWebSocketServer* wsServer_;
    QList<QWebSocket*> clients_;
};

// 在数据更新时广播
void DataManager::onRefreshTimer() {
    auto seatStatus = getCurrentSeatStatus();
    auto basicStats = getCurrentBasicStats();
    
    // 发送信号给UI
    emit seatStatusUpdated(seatStatus);
    emit basicStatsUpdated(basicStats);
    
    // 通过WebSocket广播给所有连接的客户端
    broadcastDataUpdates();
}

void DataManager::broadcastDataUpdates() {
    QJsonObject data;
    data["type"] = "seat_update";
    data["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    data["stats"] = QJsonObject::fromVariantMap(getCurrentBasicStats());
    
    QJsonDocument doc(data);
    for (auto* client : clients_) {
        client->sendTextMessage(doc.toJson());
    }

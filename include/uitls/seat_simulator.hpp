#pragma once
#include <QObject>
#include <QJsonObject>

class SeatSimulator : public QObject {
    Q_OBJECT
public:
    static SeatSimulator* instance();   // 获取单例
    Q_INVOKABLE void start();           // 开始模拟（幂等）

signals:
    void seatEvent(const QJsonObject& o);    // {type:"seat_event", seat_id, state, since}
    void seatSnapshot(const QJsonObject& o); // {type:"seat_snapshot", items:[{seat_id,state,since}...]}

private:
    explicit SeatSimulator(QObject* parent=nullptr);
    Q_DISABLE_COPY(SeatSimulator)
};

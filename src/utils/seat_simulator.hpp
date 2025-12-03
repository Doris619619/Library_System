#include "utils/seat_simulator.hpp"
#include <QTimer>
#include <QDateTime>
#include <QJsonArray>
#include <random>

namespace {
struct Seat {
    QString id;       // "S1".."S4"
    int state = 0;    // 0/1/2
    QDateTime since;  // 该状态开始时间（UTC）
};

class SeatSimCore : public QObject {
    Q_OBJECT
public:
    explicit SeatSimCore(QObject* parent=nullptr)
        : QObject(parent), rng_(std::random_device{}()), dist01_(0,99) {
        // 初始化 4 个座位
        for (int i=1;i<=4;++i) {
            Seat s;
            s.id = QString("S%1").arg(i);
            s.state = 0;
            s.since = QDateTime::currentDateTimeUtc();
            seats_.push_back(s);
        }
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &SeatSimCore::tick);
        timer_->setInterval(2000); // 2 秒一帧
    }

    void start() {
        if (!timer_->isActive()) timer_->start();
        // 启动后立即播一帧快照，方便页面首屏有数据
        emitSnapshot();
    }

signals:
    void seatEvent(const QJsonObject& o);
    void seatSnapshot(const QJsonObject& o);

private slots:
    void tick() {
        // 以小概率触发某些座位变化；概率可调
        for (auto& s : seats_) {
            int r = dist01_(rng_);
            if (r < 15) { // 15% 变化概率
                int newState = s.state;
                // 随机下一个状态，但尽量不要和当前一样
                for (int tries=0; tries<5 && newState==s.state; ++tries) {
                    newState = dist01_(rng_) % 3; // 0/1/2
                }
                if (newState != s.state) {
                    s.state = newState;
                    s.since = QDateTime::currentDateTimeUtc();

                    QJsonObject ev;
                    ev["type"]    = "seat_event";
                    ev["seat_id"] = s.id;
                    ev["state"]   = s.state;
                    ev["since"]   = s.since.toString(Qt::ISODate);
                    emit seatEvent(ev);
                }
            }
        }
        // 每 tick 都发一次全量快照
        emitSnapshot();
    }

private:
    void emitSnapshot() {
        QJsonArray items;
        for (const auto& s : seats_) {
            QJsonObject o;
            o["seat_id"] = s.id;
            o["state"]   = s.state;
            o["since"]   = s.since.toString(Qt::ISODate);
            items.push_back(o);
        }
        QJsonObject root;
        root["type"]  = "seat_snapshot";
        root["items"] = items;
        emit seatSnapshot(root);
    }

private:
    QVector<Seat> seats_;
    QTimer* timer_ = nullptr;
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist01_;
};

SeatSimCore* g_core = nullptr;
} // namespace

// ===== SeatSimulator façade =====
SeatSimulator::SeatSimulator(QObject* parent): QObject(parent) {
    if (!g_core) g_core = new SeatSimCore(this);
    // 把 core 的信号转发到对外 signal
    connect(g_core, &SeatSimCore::seatEvent,    this, &SeatSimulator::seatEvent);
    connect(g_core, &SeatSimCore::seatSnapshot, this, &SeatSimulator::seatSnapshot);
}
SeatSimulator* SeatSimulator::instance() {
    static SeatSimulator inst;
    return &inst;
}
void SeatSimulator::start() {
    if (g_core) g_core->start();
}

#include "seat_simulator.moc"

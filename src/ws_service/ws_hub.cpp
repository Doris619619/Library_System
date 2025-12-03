#include <seatui/admin/admin_window.hpp>

#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QBuffer>
#include <QImageReader>
#include <QPushButton>
#include <QDialog>
#include <QScrollArea>
#include <QPixmap>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QRandomGenerator>

#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>

#include <seatui/widgets/card_dialog.hpp>

// —— 前置声明：文件后面有它的实现（static自由函数）——
static void upsertRow(QTableWidget* t, const QString& seat, int state,
                      const QString& sinceIso, const QString& recentIso);

/* ========== 1. AdminWindow 构造 / 选项卡 ========== */

AdminWindow::AdminWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QString::fromUtf8("SeatUI 管理端"));
    resize(1100, 720);

    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);

    tabs_->addTab(buildOverviewPage(),   QString::fromUtf8("总览"));
    tabs_->addTab(buildHelpCenterPage(), QString::fromUtf8("求助中心"));
    tabs_->addTab(buildHeatmapPage(),    QString::fromUtf8("热力图"));
    tabs_->addTab(buildStatsPage(),      QString::fromUtf8("统计"));
    tabs_->addTab(buildTimelinePage(),   QString::fromUtf8("时间轴"));

    // 新增：占座监控页
    tabs_->addTab(buildSeatMonitorPage(), QString::fromUtf8("占座监控"));

    // WebSocket 服务器用于接收学生端“一键求助”
    //initWsServer();

    // === 删除原来的 initWsServer() 调用 ===
    // 改为调用 initWsClient()
    initWsClient();






    //新增
/*

    // === 订阅外部 ws_service 的座位快照（与学生端类似） ===
    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(ws_, &QWebSocket::connected, this, [this]{
        wsReady_ = true;
        // 可选：告诉服务端我是 admin
        ws_->sendTextMessage(QStringLiteral(R"({"type":"hello","role":"admin"})"));
    });
    connect(ws_, &QWebSocket::disconnected, this, [this]{
        wsReady_ = false;
        // 简单的重连（1秒后）
        QTimer::singleShot(1000, this, [this]{
            if (ws_) ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));
        });
    });
    connect(ws_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, [this](auto){
                // 首次连不上也尝试重连
                QTimer::singleShot(1000, this, [this]{
                    if (ws_ && !wsReady_) ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));
                });
            });

    // —— 订阅消息：你文件里已有 connect(ws_, &QWebSocket::textMessageReceived, ...) 这一段，保持不变 ——

    // 首次连接
    ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));

*/




    /* ========== 2. 本地演示定时器：每 2 秒刷一次座位快照 ========== */
    auto demoTimer = new QTimer(this);
    demoTimer->setInterval(2000); // 2s

    connect(demoTimer, &QTimer::timeout, this, [this](){
        // —— 若想“写死不变”，把下方四个 stX 的随机数改成固定值 0/1/2 即可 —— //
        auto rnd3 = [](){ return QRandomGenerator::global()->bounded(3); };

        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QJsonObject s1; s1["seat_id"]="S1"; s1["state"]=rnd3(); s1["since"]=now;
        QJsonObject s2; s2["seat_id"]="S2"; s2["state"]=rnd3(); s2["since"]=now;
        QJsonObject s3; s3["seat_id"]="S3"; s3["state"]=rnd3(); s3["since"]=now;
        QJsonObject s4; s4["seat_id"]="S4"; s4["state"]=rnd3(); s4["since"]=now;

        QJsonObject root; root["type"]="seat_snapshot";
        root["items"] = QJsonArray{ s1,s2,s3,s4 };

        onSeatSnapshotJson(root);
    });


/*
    connect(ws_, &QWebSocket::textMessageReceived, this, [this](const QString& msg){
    QJsonParseError er; auto doc = QJsonDocument::fromJson(msg.toUtf8(), &er);
    if (er.error != QJsonParseError::NoError || !doc.isObject()) return;
    const auto obj = doc.object();

    if (obj.value("type").toString() == "seat_snapshot"){
        const auto arr = obj.value("items").toArray();
        const QString nowUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        for (const auto& it : arr){
            if (!it.isObject()) continue;
            const auto o = it.toObject();
            const QString id    = o.value("seat_id").toString();
            const int     st    = o.value("state").toInt(0);
            const QString since = o.value("since").toString();

            setSeatCell(id, st, since);                  // 改卡片颜色和文案
            if (seatTable_) upsertRow(seatTable_, id, st, since, nowUtc);  // 改表格
        }
    }
});
*/
    //demoTimer->start();
}

/* ========== 3. 其它页面（占位） ========== */

QWidget* AdminWindow::buildOverviewPage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);
    auto t = new QLabel(QString::fromUtf8("这里展示关键 KPI（占位）：\n• 当前占用率\n• 今日异常数\n• 最近 1h 求助…"), w);
    t->setStyleSheet("font-size:15px; color:#334155;");
    v->addWidget(t);
    v->addStretch();
    return w;
}

QWidget* AdminWindow::buildHelpCenterPage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);

    helpTable_ = new QTableWidget(w);
    helpTable_->setColumnCount(6);
    helpTable_->setHorizontalHeaderLabels({QString::fromUtf8("时间(UTC)"),
                                           QString::fromUtf8("用户"),
                                           QString::fromUtf8("摘要"),
                                           QString::fromUtf8("缩略图"),
                                           QString::fromUtf8("MIME"),
                                           QString::fromUtf8("查看")});
    helpTable_->horizontalHeader()->setStretchLastSection(true);
    helpTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    helpTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    helpTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    helpTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    helpTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(helpTable_);

    auto tip = new QLabel(QString::fromUtf8("说明：学生端“一键求助”提交后，这里会出现一条记录；点击“查看”可看原图与全文。"), w);
    tip->setStyleSheet("color:#64748b;");
    v->addWidget(tip);

    return w;
}

QWidget* AdminWindow::buildHeatmapPage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);
    v->addWidget(new QLabel(QString::fromUtf8("🔥 热力图占位（后续接入 QtCharts/自绘 QImage 叠加）"), w));
    v->addStretch();
    return w;
}

QWidget* AdminWindow::buildStatsPage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);
    v->addWidget(new QLabel(QString::fromUtf8("📊 统计图占位（占用率/分区对比/小时聚合等）"), w));
    v->addStretch();
    return w;
}

QWidget* AdminWindow::buildTimelinePage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);
    v->addWidget(new QLabel(QString::fromUtf8("⏱ 时间轴/事件回放占位"), w));
    v->addStretch();
    return w;
}

/* ========== 4. 占座监控页面与逻辑 ========== */

static QString stateText(int s){
    switch (s) {
        case 1: return QString::fromUtf8("有人(1)");
        case 2: return QString::fromUtf8("有物无人(2)");
        default:return QString::fromUtf8("空(0)");
    }
}
static QString cellColorCss(int s){
    // 统一深色系：绿=有人，黄=有物无人，灰=空
    if (s == 1) return "background:#064e3b; border:1px solid #115e59; color:#d1fae5;";
    if (s == 2) return "background:#78350f; border:1px solid #92400e; color:#fde68a;";
    return       "background:#111827; border:1px solid #374151; color:#cbd5e1;";
}

QWidget* AdminWindow::buildSeatMonitorPage() {
    auto w = new QWidget(this);
    auto v = new QVBoxLayout(w);
    v->setContentsMargins(12,12,12,12);
    v->setSpacing(10);

    // 顶部：2×2 网格（S1~S4）
    auto grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    seatCells_.clear();
    seatCells_.reserve(4);

    auto makeCell = [&](const QString& id){
        auto box = new QFrame(w);
        box->setMinimumSize(140,100);
        box->setStyleSheet("QFrame{ background:#111827; border:1px solid #374151; border-radius:12px; }");

        auto ly = new QVBoxLayout(box);
        ly->setContentsMargins(12,10,12,10);
        ly->setSpacing(6);

        auto title = new QLabel(id, box);
        title->setStyleSheet("color:#e5e7eb; font-weight:600;");

        auto state = new QLabel(QString::fromUtf8("—"), box);
        state->setStyleSheet("color:#93a4b5;");

        ly->addWidget(title);
        ly->addStretch();
        ly->addWidget(state, 0, Qt::AlignRight);

        seatCells_.push_back(state);
        return box;
    };

    grid->addWidget(makeCell("S1"), 0,0);
    grid->addWidget(makeCell("S2"), 0,1);
    grid->addWidget(makeCell("S3"), 1,0);
    grid->addWidget(makeCell("S4"), 1,1);

    v->addLayout(grid);

    // 中部：当前状态表
    seatTable_ = new QTableWidget(w);
    seatTable_->setColumnCount(4);
    seatTable_->setHorizontalHeaderLabels({QString::fromUtf8("SeatID"),
                                           QString::fromUtf8("状态"),
                                           QString::fromUtf8("since(UTC)"),
                                           QString::fromUtf8("最近事件时间")});
    seatTable_->horizontalHeader()->setStretchLastSection(true);
    seatTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    seatTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    seatTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    seatTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    seatTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    seatTable_->setSortingEnabled(true);

    v->addWidget(seatTable_, 1);

    auto tip = new QLabel(QString::fromUtf8("说明：收到 seat_event/seat_snapshot 后自动刷新；颜色：绿=有人、黄=有物无人、灰=空。"), w);
    tip->setStyleSheet("color:#64748b;");
    v->addWidget(tip);

    seatPage_ = w;
    return w;
}

void AdminWindow::setSeatCell(const QString& id, int state, const QString& sinceIso){
    bool ok=false; int idx = id.mid(1).toInt(&ok);
    if (!ok || idx<1 || idx>4 || idx>seatCells_.size()) return;

    QLabel* lab = seatCells_[idx-1];
    lab->setText(stateText(state) + "\n" + sinceIso);

    if (auto box = qobject_cast<QFrame*>(lab->parentWidget())){
        box->setStyleSheet(QString("QFrame{ %1 border-radius:12px; }").arg(cellColorCss(state)));
    }
}

static void upsertRow(QTableWidget* t, const QString& seat, int state,
                      const QString& sinceIso, const QString& recentIso){
    int found = -1;
    for (int r=0; r<t->rowCount(); ++r){
        if (t->item(r,0) && t->item(r,0)->text() == seat){ found = r; break; }
    }
    if (found<0){
        int r = t->rowCount(); t->insertRow(r);
        t->setItem(r,0,new QTableWidgetItem(seat));
        t->setItem(r,1,new QTableWidgetItem(stateText(state)));
        t->setItem(r,2,new QTableWidgetItem(sinceIso));
        t->setItem(r,3,new QTableWidgetItem(recentIso));
    }else{
        t->item(found,1)->setText(stateText(state));
        t->item(found,2)->setText(sinceIso);
        t->item(found,3)->setText(recentIso);
    }
}

/* —— seat_event：单条 —— */
void AdminWindow::onSeatEventJson(const QJsonObject& o){
    const QString id    = o.value("seat_id").toString();
    const int     st    = o.value("state").toInt(0);
    const QString since = o.value("since").toString();

    setSeatCell(id, st, since);
    const QString nowUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (seatTable_) upsertRow(seatTable_, id, st, since, nowUtc);
}

/* —— seat_snapshot：批量 —— */
void AdminWindow::onSeatSnapshotJson(const QJsonObject& o){
    if (!o.contains("items") || !o.value("items").isArray()) return;
    const auto arr = o.value("items").toArray();
    const QString nowUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    for (const auto& it : arr){
        if (!it.isObject()) continue;
        const auto item = it.toObject();
        const QString id    = item.value("seat_id").toString();
        const int     st    = item.value("state").toInt(0);
        const QString since = item.value("since").toString();

        setSeatCell(id, st, since);
        if (seatTable_) upsertRow(seatTable_, id, st, since, nowUtc);
    }
}

/* ========== 5. 求助中心（WebSocket） ========== */

void AdminWindow::appendHelpRow(const QString& when, const QString& user,
                                const QString& text, const QPixmap& thumb,
                                const QByteArray& rawImgBase64, const QString& mime)
{
    const int r = helpTable_->rowCount();
    helpTable_->insertRow(r);

    auto *itemWhen = new QTableWidgetItem(when);
    auto *itemUser = new QTableWidgetItem(user);
    auto *itemSumm = new QTableWidgetItem(text.left(48) + (text.size()>48?QStringLiteral("…"):QString()));
    auto *itemMime = new QTableWidgetItem(mime);

    auto *thumbLbl = new QLabel();
    thumbLbl->setPixmap(thumb.scaled(80, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    thumbLbl->setAlignment(Qt::AlignCenter);

    auto *btn = new QPushButton(QString::fromUtf8("查看"));
    btn->setProperty("type","primary");

    helpTable_->setItem(r, 0, itemWhen);
    helpTable_->setItem(r, 1, itemUser);
    helpTable_->setItem(r, 2, itemSumm);
    helpTable_->setCellWidget(r, 3, thumbLbl);
    helpTable_->setItem(r, 4, itemMime);
    helpTable_->setCellWidget(r, 5, btn);

    connect(btn, &QPushButton::clicked, this, [=]{
        QDialog dlg(this);
        dlg.setWindowTitle(QString::fromUtf8("求助详情"));
        auto v = new QVBoxLayout(&dlg);
        auto info = new QLabel(QString(QString::fromUtf8("时间：%1\n用户：%2\nMIME：%3\n\n描述：\n%4"))
                                   .arg(when, user, mime, text), &dlg);
        info->setWordWrap(true);
        v->addWidget(info);

        if (!rawImgBase64.isEmpty()) {
            QByteArray imgBytes = QByteArray::fromBase64(rawImgBase64);
            QPixmap px; px.loadFromData(imgBytes);
            auto area = new QScrollArea(&dlg);
            auto imgL = new QLabel();
            imgL->setPixmap(px);
            area->setWidget(imgL);
            area->setWidgetResizable(true);
            area->setMinimumSize(640, 380);
            v->addWidget(area, 1);
        }

        auto ok = new QPushButton(QString::fromUtf8("知道了"), &dlg);
        ok->setProperty("type","primary");
        auto h = new QHBoxLayout(); h->addStretch(); h->addWidget(ok);
        v->addLayout(h);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        dlg.exec();
    });
}

void AdminWindow::onHelpArrived(const QByteArray& utf8Json) {
    QJsonParseError er; QJsonDocument d = QJsonDocument::fromJson(utf8Json, &er);
    if (er.error != QJsonParseError::NoError || !d.isObject()) {
        CardDialog(QString::fromUtf8("解析失败"), QString::fromUtf8("收到的求助 JSON 无法解析。"), this).exec();
        return;
    }
    const QJsonObject o = d.object();
    if (o.value("type").toString() != "student_help") return;

    const QString when = o.value("created_at").toString();
    const QString user = o.value("user").toString("student");
    const QString text = o.value("description").toString();

    QPixmap th; QByteArray rawB64; QString mime = "image/png";
    if (o.contains("image") && o.value("image").isObject()) {
        const QJsonObject im = o.value("image").toObject();
        rawB64 = im.value("base64").toString().toLatin1();
        mime   = im.value("mime").toString("image/png");
        QByteArray bytes = QByteArray::fromBase64(rawB64);
        th.loadFromData(bytes);
    }
    if (th.isNull()) { th = QPixmap(80,50); th.fill(QColor(230,235,240)); }

    appendHelpRow(when, user, text, th, rawB64, mime);
}

//删了让他作为客户端
/*
void AdminWindow::initWsServer() {
    wsServer_ = new QWebSocketServer(QStringLiteral("SeatUI-Admin-WS"),
                                     QWebSocketServer::NonSecureMode, this);
    const QHostAddress host = QHostAddress::LocalHost;  // 127.0.0.1
    const quint16 port = 12345;
    if (!wsServer_->listen(host, port)) {
        CardDialog(QString::fromUtf8("WS 启动失败"),
                   QString::fromUtf8("管理员端 WebSocket 服务器监听失败（127.0.0.1:12345）。"), this).exec();
        return;
    }

    connect(wsServer_, &QWebSocketServer::newConnection, this, [this]{
        auto *sock = wsServer_->nextPendingConnection();
        wsClients_ << sock;

        connect(sock, &QWebSocket::textMessageReceived, this, [this](const QString& msg){
            // 学生端发来的“求助”消息
            onHelpArrived(msg.toUtf8());
            // 若将来学生端/后端也会发 seat_event/seat_snapshot，这里也可解析分发：
            QJsonParseError er; QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8(), &er);
            if (er.error == QJsonParseError::NoError && d.isObject()){
                const QJsonObject o = d.object();
                const QString tp = o.value("type").toString();
                if (tp == "seat_event")    onSeatEventJson(o);
                else if (tp == "seat_snapshot") onSeatSnapshotJson(o);
            }
        });
        connect(sock, &QWebSocket::disconnected, this, [this, sock]{
            wsClients_.removeAll(sock);
            sock->deleteLater();
        });

        sock->sendTextMessage(QStringLiteral(R"({"type":"hello","role":"admin"})"));
    });
}
*/

void AdminWindow::initWsClient() {
    if (ws_) {
        ws_->deleteLater();
        ws_ = nullptr;
    }

    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    wsReady_ = false;

    connect(ws_, &QWebSocket::connected, this, [this](){
        QJsonObject hello{{"type","hello"},{"role","admin"}};
        ws_->sendTextMessage(QJsonDocument(hello).toJson(QJsonDocument::Compact));
    });

    connect(ws_, &QWebSocket::textMessageReceived, this, [this](const QString& msg){
        // 统一消息处理
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) return;

        const auto obj = doc.object();
        const QString type = obj.value("type").toString();

        if (type == "seat_snapshot") {
            onSeatSnapshotJson(obj);
        } else if (type == "student_help") {
            onHelpArrived(msg.toUtf8());
        }
        // 可以添加其他消息类型的处理


        else if (type == "seat_update") {
            const auto stats = obj.value("stats").toObject(); // 若要用统计，可读这里
            const auto arr = obj.value("seats").toArray();    // 注意是 seats
            const QString nowUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            for (const auto& it : arr) {
                if (!it.isObject()) continue;
                const auto o = it.toObject();
                const QString id = o.value("id").toString();
                const QString s  = o.value("state").toString();
                int st = (s == "Seated" ? 1 : (s == "Anomaly" ? 2 : 0));
                const QString since = o.value("last_update").toString();
                setSeatCell(id, st, since);
                if (seatTable_) upsertRow(seatTable_, id, st, since, nowUtc);
            }
        }

    });

    connect(ws_, &QWebSocket::disconnected, this, [this](){
        wsReady_ = false;
        QTimer::singleShot(1000, this, [this](){
            if (ws_) ws_->open(QUrl("ws://127.0.0.1:12345"));
        });
    });

    // 首次连接
    ws_->open(QUrl("ws://127.0.0.1:12345"));
}

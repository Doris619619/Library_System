// src/launcher_app/main.cpp
#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QHostAddress>

#include <seatui/launcher/login_window.hpp>
#include "ws/ws_hub.hpp"

// ===== 可选：如果你工程里有 VisionClient 的封装（按你提供的代码名）=====
#include <seatui/vision/VisionClient.h>   // 如果你的头文件名不同，请改为实际入口
// ===== Judger 入口 =====
#include <seatui/judger/seat_state_judger.hpp>

// ===== 数据库（单例）=====
#include "../db_core/SeatDatabase.h"
// 可选：演示数据初始化
#include "../db_core/DatabaseInitializer.h"

// ---------------- HiDPI 与全局样式 ----------------
static void initHiDpi() {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
}
static void applyGlobalStyle(QApplication& app) {
    QFont f("Microsoft YaHei UI"); f.setPointSize(10); app.setFont(f);
    const char* qss = R"(
        QWidget { font-family: "Microsoft YaHei UI"; font-size: 10pt; }
        QMainWindow { background: #f5f7fa; }
        QFrame#card { background:#ffffff; border-radius:16px; border:1px solid #e6e8eb; }
        QLabel[hint="true"] { color: #8a8f98; }
        QLineEdit {
            background:#ffffff; border:1px solid #d7dbe3; border-radius:10px; padding:8px 12px;
            selection-background-color:#0ea5e9; selection-color:#ffffff;
        }
        QLineEdit:focus { border:1px solid #0ea5e9; box-shadow:0 0 0 3px rgba(14,165,233,0.15); }
        QPushButton[type="primary"] {
            background:#0ea5e9; color:#ffffff; border:none; border-radius:10px; padding:10px 16px;
        }
        QPushButton[type="primary"]:hover { background:#0284c7; }
        QPushButton[type="primary"]:pressed{ background:#0369a1; }
        QPushButton[class="cardBtn"]{
            background:#ffffff; border:1px solid #d7dbe3; border-radius:14px; padding:14px 18px; text-align:left;
        }
        QPushButton[class="cardBtn"]:hover { border-color:#0ea5e9; box-shadow:0 4px 16px rgba(2,132,199,0.15); }
        QFrame#header { background:#ffffff; border-bottom:1px solid #e6e8eb; }
        QMessageBox { background:#ffffff; border:1px solid #e6e8eb; border-radius:12px; }
        QMessageBox QLabel { color:#1f2937; min-width:280px; }
        QMessageBox QPushButton {
            min-height:32px; padding:6px 12px; border-radius:8px; border:1px solid #d7dbe3; background:#ffffff;
        }
        QMessageBox QPushButton:hover { border-color:#0ea5e9; box-shadow:0 2px 8px rgba(2,132,199,0.15); }
        QMessageBox QPushButton[type="primary"] { background:#0ea5e9; color:#ffffff; border:none; }
        QMessageBox QPushButton[type="primary"]:hover { background:#0284c7; }
        QMessageBox QPushButton[type="primary"]:pressed{ background:#0369a1; }
    )";
    app.setStyleSheet(qss);
}

// ---------------- Vision 线程 ----------------
class VisionThread : public QThread {
    Q_OBJECT
public:
    explicit VisionThread(QObject* parent=nullptr) : QThread(parent) {}
protected:
    void run() override {
        try {
            vision::VisionClient vc;
            qInfo() << "[VisionThread] 开始运行 VisionClient …";
            // 相对“运行目录/可执行文件”的上一级：
            // 最终运行时目录形如： .../build/.../Release/
            // 因此 ../assets/vision/videos/demo.mp4 与 ../out 与你的旧代码一致。
            std::string video_path = "../../assets/vision/videos/demo.mp4";
            std::string out_jsonl  = "../../out/000000.jsonl";
            vc.runVision(
                video_path,   // 输入
                out_jsonl,    // 输出 JSONL
                "500",        // 最大帧数
                "0.5",        // FPS 抽帧
                "true"        // 流式
                );
            qInfo() << "[VisionThread] VisionClient 运行结束";
        } catch (const std::exception& e) {
            qWarning() << "[VisionThread] 异常:" << e.what();
        }
    }
};

// ---------------- Judger 线程 ----------------
class JudgerThread : public QThread {
    Q_OBJECT
public:
    explicit JudgerThread(QObject* parent=nullptr) : QThread(parent) {}
protected:
    void run() override {
        try {
            SeatStateJudger judger;
            qInfo() << "[JudgerThread] 开始运行 SeatStateJudger …";
            // 监听 ../out；内部是“无限循环扫描目录”的阻塞逻辑
            judger.run("../../out");
            qInfo() << "[JudgerThread] SeatStateJudger 结束";
        } catch (const std::exception& e) {
            qWarning() << "[JudgerThread] 异常:" << e.what();
        }
    }
};

int main(int argc, char *argv[]) {
    initHiDpi();
    QApplication app(argc, argv);

    // ==== 第一步：确保 DB 路径存在 ====
    QMessageBox::information(nullptr, "启动状态", "1. 检查数据库路径...");
    QString exeDir = QCoreApplication::applicationDirPath();
    QDir dir(exeDir);
    if (!dir.cdUp()) {
        QMessageBox::critical(nullptr, "错误", "无法访问上级目录"); return -1;
    }
    //const QString outputDir = dir.path() + "/out";


    // const QString outputDir = QDir::cleanPath("D:/CSC3002/Library_System/build_vs_all/out");
    const QString outputDir = QDir::cleanPath("D:/CSC3002/Library_System/out");
    const QString dbPath    = outputDir + "/seating_system.db";
    auto& db = SeatDatabase::getInstance(dbPath.toStdString()); // 保持显式传入

    //const QString outputDir = QDir::cleanPath("D:/CSC3002/Library_System/build_vs_all/out");
    //const QString dbPath    = outputDir + "/seating_system.db";
    if (!QDir().mkpath(outputDir)) {
        QMessageBox::critical(nullptr, "错误", "无法创建目录: " + outputDir); return -1;
    }
    if (!QFile::exists(dbPath)) { QFile f(dbPath); if (!f.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(nullptr, "错误", "无法创建数据库文件"); return -1; } f.close(); }

    // ==== 第二步：初始化 DB（建表） ====
    QMessageBox::information(nullptr, "启动状态", "2. 初始化数据库...");
    try {
        auto& db = SeatDatabase::getInstance(dbPath.toStdString());
        if (!db.initialize()) {
            QMessageBox::critical(nullptr,"数据库错误","数据库初始化失败！"); return -1;
        }

        // （可选）初始化演示数据：需要时解除注释
        // DatabaseInitializer init(db);
        // init.initializeSampleData();

        QMessageBox::information(nullptr, "启动状态", "3. 数据库初始化成功");
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "异常", QString("数据库异常: %1").arg(e.what())); return -1;
    }

    // Qt：最后一个窗口关闭时退出
    app.setQuitOnLastWindowClosed(true);
    applyGlobalStyle(app);

    // ==== 第三步：启动内置 WS 服务（推送 seat_update） ====
    QMessageBox::information(nullptr, "启动状态", "4. 启动 WebSocket 服务...");
    static WsHub* wsHub = new WsHub(&app);
    const quint16 port = 12345;
    if (!wsHub->start(port, QHostAddress::LocalHost)) {
        QMessageBox::warning(nullptr, "WebSocket警告",
                             QString("WebSocket服务启动失败（端口%1可能被占用），程序继续运行").arg(port));
    } else {
        QMessageBox::information(nullptr, "启动状态", "5. WebSocket服务启动成功");
    }

    // ==== 第四步：准备 out 目录 & 启动 Vision / Judger 线程 ====
    // out 目录与之前“独立 main”的代码保持完全一致
    {
        QDir outDir(dir.path());
        if (!outDir.exists("out")) outDir.mkdir("out");
    }
    auto* visionThread = new VisionThread(&app);
    auto* judgerThread = new JudgerThread(&app);
    visionThread->start();  // 非阻塞
    judgerThread->start();  // 非阻塞（内部为阻塞轮询）

    // ==== 第五步：一次创建两个“登录窗口” ====
    QMessageBox::information(nullptr, "启动状态", "6. 创建登录窗口...");
    auto *login1 = new LoginWindow(nullptr);
    login1->setAttribute(Qt::WA_DeleteOnClose);
    login1->setWindowTitle(QStringLiteral("登录窗口 #1"));
    login1->show();

    auto *login2 = new LoginWindow(nullptr);
    login2->setAttribute(Qt::WA_DeleteOnClose);
    login2->setWindowTitle(QStringLiteral("登录窗口 #2"));
    login2->show();

    if (auto *scr = QGuiApplication::primaryScreen()) {
        const QRect r = scr->availableGeometry();
        const int w = 520, h = 380;
        login1->setGeometry(r.center().x() - w - 16, r.center().y() - h/2, w, h);
        login2->setGeometry(r.center().x() + 16,     r.center().y() - h/2, w, h);
    }

    QMessageBox::information(nullptr, "启动状态", "7. 窗口创建完成，进入主循环");

    // 退出时尝试结束后台线程（Judger 内部是无限循环，不保证能在此处优雅退出）
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        if (visionThread->isRunning()) { visionThread->quit(); visionThread->wait(1000); }
        // judgerThread 内部为一直轮询目录的阻塞 run()，随进程退出
    });

    return app.exec();
}

#include "main.moc"

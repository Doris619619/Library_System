#pragma once

#include <QMainWindow>
#include <QList>
#include <QVector>

class QTabWidget;
class QTableWidget;
class QLabel;
class QWidget;
class QWebSocketServer;
class QWebSocket;

#include <QJsonObject>

class AdminWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit AdminWindow(QWidget* parent = nullptr);

private: // —— 页面构建 —— //
    QWidget* buildOverviewPage();
    QWidget* buildHelpCenterPage();
    QWidget* buildHeatmapPage();
    QWidget* buildStatsPage();
    QWidget* buildTimelinePage();

    // 新增：占座监控页
    QWidget* buildSeatMonitorPage();

private: // —— 逻辑/工具 —— //
    void appendHelpRow(const QString& when,
                       const QString& user,
                       const QString& text,
                       const QPixmap& thumb,
                       const QByteArray& rawImgBase64,
                       const QString& mime);
    void initWsServer();

    // 新增：占座监控用
    void setSeatCell(const QString& id, int state, const QString& sinceIso);
    void onSeatEventJson(const QJsonObject& o);     // 单条 seat_event
    void onSeatSnapshotJson(const QJsonObject& o);  // 批量 seat_snapshot

private: // —— 成员 —— //
    QTabWidget*    tabs_ = nullptr;

    // 求助中心
    QTableWidget*  helpTable_ = nullptr;
    QWebSocketServer* wsServer_ = nullptr;
    QList<QWebSocket*> wsClients_;

    // 占座监控
    QVector<QLabel*>  seatCells_;     // 2×2 网格中右下角的状态标签
    QTableWidget*     seatTable_ = nullptr; // 当前状态表
    QWidget*          seatPage_  = nullptr; // 页面根（可用于整体刷新/样式）
    
private slots:
    void onHelpArrived(const QByteArray& utf8Json);
};

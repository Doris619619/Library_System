#include <seatui/student/student_window.hpp>
#include <seatui/launcher/login_window.hpp>
#include <seatui/student/navigation_canvas.hpp>
#include <QCheckBox>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QTextEdit>
#include <QFileDialog>
#include <QImageReader>
#include <QBuffer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QScrollArea>

#include <QMimeDatabase>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QDebug>
#include <seatui/widgets/card_dialog.hpp>

#include <seatui/student/book_search.hpp>

#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>

#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

//文件读取
static QString locateBooksFile() {
    // 1) 运行目录 /Input/books.txt  —— CMake 已拷贝
    QStringList candidates;
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath("Input/books.txt");

    // 2) 兼容某些构建目录层级（上一层、上两层）
    candidates << QDir(appDir + "/..").filePath("Input/books.txt");
    candidates << QDir(appDir + "/../..").filePath("Input/books.txt");

    // 3) 当前工作目录（少数IDE会把 cwd 设置为别处）
    candidates << QDir::current().filePath("Input/books.txt");

    // 4) 源码相对路径兜底：如果你仍然把 books.txt 放在 src/student_app/ 旁边（不推荐）
    //    这里也给你一个兜底（请尽快改回放到 项目根/Input/）
    candidates << QDir(appDir).filePath("../src/student_app/books.txt");

    for (const QString& p : candidates) {
        if (QFileInfo::exists(p)) return QFileInfo(p).absoluteFilePath();
    }
    return QString(); // 没找到
}


// 侧边栏通用按钮
static QPushButton* makeSideBtn(const QString& text, QWidget* parent) {
    auto *b = new QPushButton(text, parent);
    b->setCheckable(true);
    b->setMinimumHeight(40);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(
        "QPushButton{ text-align:left; padding:8px 12px; border:0; "
        " border-radius:8px; color:#e5e7eb; background:transparent; }"
        "QPushButton:hover{ background:rgba(255,255,255,0.06);} "
        "QPushButton:checked{ background:rgba(59,130,246,0.18); color:#fff; }"
        );
    return b;
}

StudentWindow::StudentWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(u8"SeatUI 学生端");
    resize(1000, 680);

    // ===== 左侧侧边栏 =====
    auto side = new QFrame(this);
    side->setFixedWidth(190);
    side->setStyleSheet("QFrame{ background:#0f172a; border-right:1px solid #1f2937; }");
    auto sideLy = new QVBoxLayout(side);
    sideLy->setContentsMargins(12,16,12,16);
    sideLy->setSpacing(10);

    // 顶部"返回登录"（非互斥按钮，不高亮选中状态）
    auto btnBack = new QPushButton(u8"← 返回登录", side);
    btnBack->setCursor(Qt::PointingHandCursor);
    btnBack->setStyleSheet(
        "QPushButton{ text-align:left; padding:8px 12px; border:0; border-radius:8px; "
        "  color:#cbd5e1; background:rgba(255,255,255,0.04);} "
        "QPushButton:hover{ background:rgba(255,255,255,0.10);} "
        "QPushButton:pressed{ background:rgba(37,99,235,0.25); color:#fff; }"
        );
    sideLy->addWidget(btnBack);

    auto title = new QLabel(u8"学生端", side);
    title->setStyleSheet("color:#cbd5e1; font-weight:600; padding:4px;");
    sideLy->addWidget(title);

    btnDash = makeSideBtn(u8"🏠 仪表盘", side);
    btnNav  = makeSideBtn(u8"🧭 导航", side);
    btnHeat = makeSideBtn(u8"🔥 热力图", side);
    btnHelp = makeSideBtn(u8"🆘 一键求助", side);
    btnLive = makeSideBtn(u8"💺 座位实况", side);
    btnBook = makeSideBtn(u8"📚 图书查询", side);

    // 按钮互斥
    btnDash->setAutoExclusive(true);
    btnNav->setAutoExclusive(true);
    btnHeat->setAutoExclusive(true);
    btnHelp->setAutoExclusive(true);
    btnLive->setAutoExclusive(true);
    btnBook->setAutoExclusive(true);

    sideLy->addWidget(btnDash);
    sideLy->addWidget(btnNav);
    sideLy->addWidget(btnHeat);
    sideLy->addWidget(btnHelp);
    sideLy->addWidget(btnLive);
    sideLy->addWidget(btnBook);
    sideLy->addStretch();

    // ===== 右侧页面区（堆叠）=====
    pages = new QStackedWidget(this);
    pages->addWidget(buildDashboardPage());   // 0
    pages->addWidget(buildNavigationPage());  // 1
    pages->addWidget(buildHeatmapPage());     // 2
    pages->addWidget(buildHelpPage());        // 3
    pages->addWidget(buildLivePage());        // 4
    pages->addWidget(buildBookSearchPage());  // 5 - 书籍搜索页面


    // 默认落在仪表盘
    pages->setCurrentIndex(0);
    btnDash->setChecked(true);

    // 根布局：左侧栏 + 右侧页面
    auto central = new QWidget(this);
    auto root = new QHBoxLayout(central);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addWidget(side);
    root->addWidget(pages, 1);
    setCentralWidget(central);

    // 侧边栏信号
    connect(btnBack, &QPushButton::clicked, this, &StudentWindow::onBackToLogin);
    connect(btnDash, &QPushButton::clicked, this, &StudentWindow::gotoDashboard);
    connect(btnNav,  &QPushButton::clicked, this, &StudentWindow::gotoNavigation);
    connect(btnHeat, &QPushButton::clicked, this, &StudentWindow::gotoHeatmap);
    connect(btnHelp, &QPushButton::clicked, this, &StudentWindow::gotoHelp);
    connect(btnLive, &QPushButton::clicked, this, &StudentWindow::gotoLive);
    connect(btnBook, &QPushButton::clicked, this, &StudentWindow::gotoBookSearch);

    initWsClient();
}

/* ---------- 页面构建 ---------- */

QWidget* StudentWindow::buildDashboardPage() {
    auto page = new QWidget(this);
    page->setStyleSheet("background:#111827;");

    auto ly = new QVBoxLayout(page);
    ly->setContentsMargins(20,20,20,20);
    ly->setSpacing(14);

    auto h = new QLabel(u8"👋 欢迎！在左侧选择功能：导航 / 热力图（后续可继续添加）。", page);
    h->setStyleSheet("color:#e5e7eb; font-size:16px;");
    ly->addWidget(h);

    auto tip = new QLabel(u8"• 导航：输入 A/B/C/D 生成发光路径\n• 热力图：占位，后续接入", page);
    tip->setStyleSheet("color:#9ca3af;");
    ly->addWidget(tip);
    ly->addStretch();
    return page;
}

QWidget* StudentWindow::buildNavigationPage() {
    auto page = new QWidget(this);

    // —— 深色主题的完整样式（让文字/边框"看得见"） —— //
    page->setStyleSheet(
        "QWidget{ background:#0b1220; }"
        "QLabel{ color:#cbd5e1; }"

        // 下拉
        "QComboBox{ color:#e5e7eb; background:#0f172a; "
        "  border:1px solid #374151; border-radius:6px; padding:4px 8px; min-width:80px; }"
        "QComboBox::drop-down{ width:22px; }"
        "QComboBox QAbstractItemView{ background:#111827; color:#e5e7eb; "
        "  selection-background-color:#2563eb; selection-color:#ffffff; }"

        // 按钮
        "QPushButton{ color:#e5e7eb; background:#1f2937; "
        "  border:1px solid #374151; border-radius:8px; padding:6px 12px; }"
        "QPushButton:hover{ background:#374151; }"
        "QPushButton:pressed{ background:#2563eb; border-color:#2563eb; }"
        "QPushButton:disabled{ color:#7c8794; background:#151a22; border-color:#2b3340; }"

        // 画布
        "#mapFrame{ background:#101319; border:1px solid #374151; border-radius:12px; }"
        );

    auto root = new QVBoxLayout(page);
    root->setContentsMargins(20,20,20,20);
    root->setSpacing(12);

    // 顶部控制条
    auto ctrl = new QHBoxLayout();
    ctrl->setSpacing(10);
    auto destLabel = new QLabel(u8"目标书架：", page);
    destBox = new QComboBox(page);
    destBox->addItems({u8"A", u8"B", u8"C", u8"D"});
    btnGen   = new QPushButton(u8"生成路径", page);
    btnClear = new QPushButton(u8"清除", page);

    ctrl->addWidget(destLabel);
    ctrl->addWidget(destBox);
    ctrl->addSpacing(12);
    ctrl->addWidget(btnGen);
    ctrl->addWidget(btnClear);
    ctrl->addStretch();

    auto ssaaBox = new QCheckBox(u8"高质量抗锯齿(2×)", page);
    ssaaBox->setChecked(true);
    ctrl->addSpacing(12);
    ctrl->addWidget(ssaaBox);

    // 地图画布占位
    auto canvasWidget = new NavigationCanvas(page);
    canvasWidget->setObjectName("mapFrame");   // 复用样式边框
    navCanvas = canvasWidget;

    connect(ssaaBox, &QCheckBox::toggled, canvasWidget, &NavigationCanvas::setSuperSample);

    // 底部状态
    navStatus = new QLabel(u8"提示：选择 A/B/C/D，点击\"生成路径\"。", page);

    navStatus->setStyleSheet("color:#93a4b5;");

    // 布局安装
    root->addLayout(ctrl);
    root->addWidget(navCanvas, 1);
    root->addWidget(navStatus);

    // 信号槽
    connect(btnGen,   &QPushButton::clicked, this, &StudentWindow::onGenerate);
    connect(btnClear, &QPushButton::clicked, this, &StudentWindow::onClear);

    // 快捷键
    btnGen->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    btnClear->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    // 确保未被禁用（防止其他地方误设）
    btnGen->setEnabled(true);
    btnClear->setEnabled(true);
    destBox->setEnabled(true);

    return page;
}

QWidget* StudentWindow::buildHeatmapPage() {
    auto page = new QWidget(this);
    page->setStyleSheet("background:#0b1220;");
    auto ly = new QVBoxLayout(page);
    ly->setContentsMargins(20,20,20,20);

    auto lbl = new QLabel(u8"🔥 热力图占位页（后续接入）", page);
    lbl->setStyleSheet("color:#e5e7eb; font-weight:600;");
    auto box = new QFrame(page);
    box->setMinimumSize(680,440);
    box->setStyleSheet("QFrame{ background:#111827; border:1px dashed #374151; border-radius:12px;}");

    ly->addWidget(lbl);
    ly->addWidget(box, 1);
    return page;
}

QWidget* StudentWindow::buildBookSearchPage() {
    auto page = new QWidget(this);
    page->setStyleSheet("QWidget{ background:#0b1220; } QLabel{ color:#cbd5e1; }");

    auto root = new QVBoxLayout(page);
    root->setContentsMargins(20,20,20,20);
    root->setSpacing(12);

    auto title = new QLabel(u8"📚 图书查询", page);
    title->setStyleSheet("font-weight:600; font-size:16px;");
    root->addWidget(title);

    // 顶部输入行：提示"作者 或 书名 关键字"
    auto row = new QHBoxLayout();
    row->setSpacing(8);
    auto lab = new QLabel(u8"关键词：", page);
    bookInput = new QLineEdit(page);
    bookInput->setPlaceholderText(u8"输入作者或书名的一部分，例如：Tanenbaum / Data / Prata …");
    
    // 输入框：文字白色、placeholder 浅灰、选中背景、边框
bookInput->setStyleSheet(
    "QLineEdit{"
    "  color:#e5e7eb;"                    /* 文本白-浅 */
    "  background:#0f172a;"               /* 深色背景（可与你的页面背景一致） */
    "  border:1px solid #94a3b8;"
    "  border-radius:12px;"
    "  padding:8px 12px;"
    "  selection-background-color:#334155;"/* 选中文本底色 */
    "}"
    "QLineEdit::placeholder{"
    "  color:#9ca3af;"                    /* placeholder 浅灰 */
    "}"
);

// 兼容：有些平台/样式下 placeholder 颜色需要通过 QPalette 指定
{
    QPalette pal = bookInput->palette();
    pal.setColor(QPalette::Text, QColor("#e5e7eb"));               // 正文字
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    pal.setColor(QPalette::PlaceholderText, QColor("#9ca3af"));    // Qt 6
#endif
    bookInput->setPalette(pal);
}

    
    bookSearchBtn = new QPushButton(u8"搜索", page);


    bookSearchBtn->setStyleSheet(
    "QPushButton{"
    "  color:#e5e7eb;"
    "  background:#1f2937;"
    "  border:1px solid #334155;"
    "  border-radius:10px;"
    "  padding:6px 14px;"
    "}"
    "QPushButton:hover{"
    "  background:#223047;"
    "}"
    "QPushButton:pressed{"
    "  background:#1b2638;"
    "}"
    );



    bookSearchBtn->setProperty("type","primary");
    row->addWidget(lab);
    row->addWidget(bookInput, 1);
    row->addWidget(bookSearchBtn);
    root->addLayout(row);

    // 结果表（九列，逐项展示完整信息）
    bookTable = new QTableWidget(page);
    bookTable->setColumnCount(9);
    bookTable->setHorizontalHeaderLabels({
        "ISBN","Title","Author","Publisher","Date","Category","CallNumber","Total","Available"
    });
    bookTable->horizontalHeader()->setStretchLastSection(true);
    bookTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    bookTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bookTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(bookTable, 1);

    // 命中为空时的提示
    bookHint = new QLabel(u8"暂无此书籍。", page);
    bookHint->setStyleSheet("color:#93a4b5;");
    bookHint->setVisible(false);
    root->addWidget(bookHint);

    // 连接搜索按钮
    connect(bookSearchBtn, &QPushButton::clicked, this, &StudentWindow::onSearchBooks);

    // 修改部分开始 - 替换原来的书籍文件加载代码
    if (!bookEngine.ready()) {
        const QString path = locateBooksFile();  // 使用 locateBooksFile() 函数获取路径
        QString err;
        if (!path.isEmpty()) {
            bookEngine.loadFromFile(path, &err); // 失败先不弹，onSearchBooks 再提示
        }
    }
    // 修改部分结束

    bookPage = page;
    return page;
}

void StudentWindow::onSearchBooks() {
    const QString kw = bookInput ? bookInput->text().trimmed() : QString();
    if (kw.isEmpty()) {
        CardDialog(u8"提示", u8"请输入作者或书名关键词。", this).exec();
        return;
    }

    // 若尚未加载，尝试再加载一次（防止首次进入失败）
    if (!bookEngine.ready()) {
    const QString path = locateBooksFile();
    QString err;
    if (path.isEmpty()) {
        CardDialog(u8"读取失败",
                   u8"未找到 books.txt。\n"
                   u8"请确认已将文件放在 项目根目录/Input/books.txt，"
                   u8"并且 CMake 已执行拷贝到运行目录。",
                   this).exec();
        return;
    }
    if (!bookEngine.loadFromFile(path, &err)) {
        CardDialog(u8"读取失败",
                   u8"无法读取：\n" + path + u8"\n\n错误：" + err,
                   this).exec();
        return;
    }
}


    const auto matches = bookEngine.searchByKeyword(kw);
    bookTable->setRowCount(0);

    if (matches.isEmpty()) {
        bookHint->setVisible(true);  // 显示"暂无此书籍。"
        return;
    }
    bookHint->setVisible(false);

    bookTable->setRowCount(matches.size());
    for (int i=0;i<matches.size();++i) {
        const auto& b = matches[i];
        auto set = [&](int c, const QString& text){
            auto *it = new QTableWidgetItem(text);
            bookTable->setItem(i, c, it);
        };
        set(0, b.isbn);
        set(1, b.title);
        set(2, b.author);
        set(3, b.publisher);
        set(4, b.date);
        set(5, b.category);
        set(6, b.callNumber);
        set(7, QString::number(b.total));
        set(8, QString::number(b.available));
    }
}

QWidget* StudentWindow::buildHelpPage() {
    auto page = new QWidget(this);
    page->setStyleSheet(
        "QWidget{ background:#0b1220; }"
        "QLabel{ color:#cbd5e1; }"
        "QTextEdit{ color:#e5e7eb; background:#0f172a; border:1px solid #374151; "
        "  border-radius:8px; padding:8px 10px; }"
        "QPushButton{ color:#e5e7eb; background:#1f2937; border:1px solid #374151; "
        "  border-radius:8px; padding:6px 12px; }"
        "QPushButton:hover{ background:#374151; }"
        "QPushButton:pressed{ background:#2563eb; border-color:#2563eb; }"
        "#imgBox{ background:#101319; border:1px dashed #374151; border-radius:12px; }"
        );

    auto root = new QVBoxLayout(page);
    root->setContentsMargins(20,20,20,20);
    root->setSpacing(12);

    auto title = new QLabel(u8"🆘 一键求助", page);
    title->setStyleSheet("color:#e5e7eb; font-weight:600; font-size:16px;");
    root->addWidget(title);

    auto tip = new QLabel(u8"请描述你的问题（可选附图）。提交后管理员端将实时收到。", page);
    tip->setStyleSheet("color:#93a4b5;");
    root->addWidget(tip);

    // —— 文本描述 —— //
    helpText_ = new QTextEdit(page);
    helpText_->setPlaceholderText(u8"例如：自习区有人高声通话 / 插座损坏 / 座位被物品长期占用…（必填其一：文字或图片）");
    helpText_->setMinimumHeight(120);
    root->addWidget(helpText_);

    // —— 图片区域：预览 + 选择 —— //
    auto imgRow = new QHBoxLayout();
    imgRow->setSpacing(12);

    auto imgBox = new QFrame(page);
    imgBox->setObjectName("imgBox");
    imgBox->setMinimumSize(220, 160);
    auto imgLy = new QVBoxLayout(imgBox);
    imgLy->setContentsMargins(12,12,12,12);
    imgLy->setSpacing(8);

    helpImgPreview_ = new QLabel(imgBox);
    helpImgPreview_->setAlignment(Qt::AlignCenter);
    helpImgPreview_->setText(u8"（无图片）");
    helpImgPreview_->setStyleSheet("color:#66758a;");
    helpImgPreview_->setMinimumHeight(120);
    imgLy->addWidget(helpImgPreview_, 1);

    helpPickBtn_ = new QPushButton(u8"选择图片…", imgBox);
    imgLy->addWidget(helpPickBtn_, 0, Qt::AlignRight);

    imgRow->addWidget(imgBox, 0);

    imgRow->addStretch();
    root->addLayout(imgRow);

    // —— 操作区 —— //
    auto op = new QHBoxLayout();
    op->addStretch();
    helpResetBtn_  = new QPushButton(u8"重置", page);
    helpSubmitBtn_ = new QPushButton(u8"提交", page); helpSubmitBtn_->setEnabled(true);
    op->addWidget(helpResetBtn_);
    op->addWidget(helpSubmitBtn_);
    root->addLayout(op);

    // —— 事件 —— //
    connect(helpPickBtn_,  &QPushButton::clicked, this, &StudentWindow::onPickImage);
    connect(helpResetBtn_, &QPushButton::clicked, this, &StudentWindow::onResetHelp);
    connect(helpSubmitBtn_,&QPushButton::clicked, this, &StudentWindow::onSubmitHelp);

    return page;
}

QWidget* StudentWindow::buildLivePage() {
    // ① 页面底色和字色：延续你当前的深色系风格
    auto page = new QWidget(this);
    page->setStyleSheet("QWidget{ background:#0b1220; } QLabel{ color:#cbd5e1; }");

    // ② 顶层纵向布局
    auto root = new QVBoxLayout(page);
    root->setContentsMargins(20,20,20,20);
    root->setSpacing(12);

    // ③ 标题
    auto title = new QLabel(u8"座位实况（Demo：2×2，S1~S4）", page);
    title->setStyleSheet("font-weight:600; font-size:16px;");
    root->addWidget(title);

    // ④ 2×2 宫格
    auto grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    liveCells_.clear();
    liveCells_.reserve(4);

    // 工厂函数：做一个"卡片"
    auto makeCell = [&](const QString& id){
        auto box = new QFrame(page);
        box->setMinimumSize(160,120);
        box->setStyleSheet("QFrame{ background:#101319; border:1px solid #374151; border-radius:12px; }");

        auto v = new QVBoxLayout(box);
        v->setContentsMargins(12,12,12,12);
        v->setSpacing(6);

        auto head = new QLabel(QString(u8"座位 %1").arg(id), box);
        head->setStyleSheet("color:#e5e7eb; font-weight:600;");

        // 右下角状态（初始显示 —）
        auto body = new QLabel(u8"—", box);
        body->setStyleSheet("color:#93a4b5;");

        v->addWidget(head);
        v->addStretch();
        v->addWidget(body, 0, Qt::AlignRight);

        liveCells_.push_back(body);   // 记录下来，后面按索引设置
        return box;
    };

    grid->addWidget(makeCell("S1"), 0,0);
    grid->addWidget(makeCell("S2"), 0,1);
    grid->addWidget(makeCell("S3"), 1,0);
    grid->addWidget(makeCell("S4"), 1,1);

    root->addLayout(grid, 1);

    // ⑤ 提示说明
    auto tip = new QLabel(u8"颜色：绿=有人、黄=占座(有物无人)、灰=没人；下方文字为状态与 since（演示先写死）。", page);
    tip->setStyleSheet("color:#66758a;");
    root->addWidget(tip);

    // ⑥ ——演示：进入页面时直接"写死"一组状态——
    //   S1=有人(1) S2=占座(2) S3=没人(0) S4=有人(1)
    const QString demoSince = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    liveSetCell("S1", 1, demoSince);
    liveSetCell("S2", 2, demoSince);
    liveSetCell("S3", 0, demoSince);
    liveSetCell("S4", 1, demoSince);

    livePage = page;
    return page;
}

/* ---------- 侧边栏切换 ---------- */
void StudentWindow::gotoDashboard() { 
    pages->setCurrentIndex(0); 
    btnDash->setChecked(true); 
}

void StudentWindow::gotoNavigation(){ 
    pages->setCurrentIndex(1); 
    btnNav->setChecked(true);  
}

void StudentWindow::gotoHeatmap()   { 
    pages->setCurrentIndex(2); 
    btnHeat->setChecked(true); 
}

void StudentWindow::gotoHelp() { 
    pages->setCurrentIndex(3); 
    btnHelp->setChecked(true); 
}

void StudentWindow::gotoLive() {
    if (livePage) {
        pages->setCurrentWidget(livePage);
        if (btnLive) btnLive->setChecked(true);
    }
}

void StudentWindow::gotoBookSearch() {
    if (bookPage) {
        pages->setCurrentWidget(bookPage);
        if (btnBook) btnBook->setChecked(true);
    }
}

/* ---------- 返回登录 ---------- */
#include <QTimer>

void StudentWindow::onBackToLogin() {
    this->hide();
    QTimer::singleShot(0, this, [this]{
        auto *login = new LoginWindow();
        login->setAttribute(Qt::WA_DeleteOnClose);
        login->show();
        this->deleteLater();
    });
}

/* ---------- 导航页按钮占位逻辑（下一步接绘制） ---------- */
void StudentWindow::onGenerate() {
    const QString dest = destBox->currentText();
    navStatus->setText(u8"已设置目标： " + dest + u8"（下一步在画布上绘制发光贝塞尔路径与粒子）");
    // TODO：在 navCanvas 上绘制路径
}

void StudentWindow::onClear() {
    navStatus->setText(u8"已清除路径。");
    // TODO：清空 navCanvas
}

void StudentWindow::onPickImage() {
    const QString file = QFileDialog::getOpenFileName(
        this, u8"选择图片",
        QString(),
        u8"图像文件 (*.png *.jpg *.jpeg *.bmp *.gif)"
        );
    if (file.isEmpty()) return;

    QImageReader reader(file);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        CardDialog(u8"读取失败", u8"无法读取该图片文件。", this).exec(); // 复用你的卡片弹框
        return;
    }

    // 预览：自适应缩放
    const int maxW = 360, maxH = 200;
    QImage scaled = img.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    helpImgPreview_->setPixmap(QPixmap::fromImage(scaled));
    helpImgPreview_->setText(QString());

    // 编码：优先 PNG
    QByteArray bytes;
    {
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG", 6);
    }
    helpImgBytes_    = bytes;
    helpImgFilename_ = QFileInfo(file).fileName();
    helpImgMime_     = "image/png";
}

void StudentWindow::onResetHelp() {
    helpText_->clear();
    helpImgPreview_->setPixmap(QPixmap());
    helpImgPreview_->setText(u8"（无图片）");
    helpImgBytes_.clear();
    helpImgFilename_.clear();
    helpImgMime_.clear();
}

void StudentWindow::onSubmitHelp() {
    const QString desc = helpText_->toPlainText().trimmed();

    if (desc.isEmpty() && helpImgBytes_.isEmpty()) {
        CardDialog(u8"内容为空", u8"请至少填写文字或选择一张图片。", this).exec();
        return;
    }

    // —— 组装 JSON 载荷 —— //
    QJsonObject root;
    root["type"] = "student_help";
    root["user"] = "student"; // 可替换成登录用户名/UID
    root["description"] = desc;
    root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    if (!helpImgBytes_.isEmpty()) {
        QJsonObject img;
        img["filename"] = helpImgFilename_.isEmpty() ? "help.png" : helpImgFilename_;
        img["mime"]     = helpImgMime_.isEmpty() ? "image/png" : helpImgMime_;
        img["base64"]   = QString::fromLatin1(helpImgBytes_.toBase64());
        root["image"]   = img;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    // qDebug() << "HELP JSON:" << payload;

    // —— 发送到管理员端 —— //
    wsSend(payload);

    // 成功提示
    CardDialog(u8"已提交", u8"你的求助信息已发送，管理员会尽快处理。", this).exec();

    // 清空
    onResetHelp();
}

void StudentWindow::initWsClient() {
    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    ws_->ignoreSslErrors();  // 非 TLS
    wsReady_ = false;

    connect(ws_, &QWebSocket::connected, this, [this]{
        wsReady_ = true;
        // 可选：握手
        ws_->sendTextMessage(QStringLiteral(R"({"type":"hello","role":"student"})"));
    });
    connect(ws_, &QWebSocket::disconnected, this, [this]{
        wsReady_ = false;
        // 简单重连（本机单进程足够稳定，失连时延时重连）
        QTimer::singleShot(1000, this, [this]{
            ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));
        });
    });
    connect(ws_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, [this](auto){
                // 首次连不上也尝试重连
                QTimer::singleShot(1000, this, [this]{
                    if (ws_ && !wsReady_) ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));
                });
            });

    // 首次连接
    ws_->open(QUrl(QStringLiteral("ws://127.0.0.1:12345")));
}

void StudentWindow::wsSend(const QByteArray& utf8Json) {
    if (ws_ && wsReady_) {
        ws_->sendTextMessage(QString::fromUtf8(utf8Json));
    } else {
        // 兜底：连不上就提醒（不丢数据也行：可选入本地队列/DB）
        CardDialog(u8"未连接", u8"尚未连接管理员端（WS）。稍后将自动重试。", this).exec();
    }
}

// 小工具：状态 → 文案
static QString demoStateText(int s){
    switch (s) {
        case 1: return QStringLiteral("有人(1)");
        case 2: return QStringLiteral("占座(2)");   // 有物无人
        default:return QStringLiteral("没人(0)");
    }
}

// 小工具：状态 → 颜色（卡片底色 + 文字色）
static QString demoCellCss(int s){
    if (s == 1) return "QFrame{ background:#064e3b; border:1px solid #115e59; border-radius:12px; } QLabel{ color:#d1fae5; }"; // 绿
    if (s == 2) return "QFrame{ background:#78350f; border:1px solid #92400e; border-radius:12px; } QLabel{ color:#fde68a; }"; // 黄
    return       "QFrame{ background:#101319; border:1px solid #374151; border-radius:12px; } QLabel{ color:#cbd5e1; }";        // 灰
}

void StudentWindow::liveSetCell(const QString& id, int state, const QString& sinceIso){
    // 把 "S1" → 1，索引=1-1=0
    bool ok=false; int idx = id.mid(1).toInt(&ok);
    if (!ok || idx<1 || idx>4 || idx>liveCells_.size()) return;

    QLabel* lab = liveCells_[idx-1];
    lab->setText(demoStateText(state) + "\n" + sinceIso);

    // 给"外层卡片 QFrame"换底色
    if (auto box = qobject_cast<QFrame*>(lab->parentWidget())){
        box->setStyleSheet(demoCellCss(state));
    }
}

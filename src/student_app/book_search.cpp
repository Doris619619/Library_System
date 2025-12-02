#include <seatui/student/book_search.hpp>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDebug>  // 为了输出调试信息
#include <QDir>    // 为了处理路径

#include <QStringConverter>   // 一定要有（Qt 6）

#include <QFileInfo>







static inline bool containsCaseInsensitive(const QString& haystack, const QString& needle) {
    return haystack.contains(needle, Qt::CaseInsensitive);
}

QString BookSearchEngine::trim(const QString& s) {
    QString t = s;
    return t.trimmed();
}

/*
bool BookSearchEngine::loadFromFile(const QString& filePath, QString* errMsg) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errMsg) *errMsg = QStringLiteral("无法打开文件：%1").arg(filePath);
        loaded_ = false;
        items_.clear();
        return false;
    }

    */

bool BookSearchEngine::loadFromFile(const QString& filePath, QString* errMsg) {
    QString p = filePath;
    if (p.isEmpty()) {
        if (errMsg) *errMsg = QStringLiteral("未提供文件路径。");
        loaded_ = false;
        items_.clear();
        return false;
    }
    if (!QFileInfo::exists(p)) {
        if (errMsg) *errMsg = QStringLiteral("文件不存在：%1").arg(p);
        loaded_ = false;
        items_.clear();
        return false;
    }

    QFile f(p);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errMsg) *errMsg = QStringLiteral("无法打开文件：%1").arg(p);
        loaded_ = false;
        items_.clear();
        return false;
    }

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8); // Qt6 正确写法

    items_.clear();
    BookItem cur;
    bool hasData = false;

    while (!in.atEnd()) {
        const QString raw = in.readLine();
        const QString line = trim(raw);
        if (line.isEmpty()) {
            if (hasData && !cur.title.isEmpty()) {
                items_.push_back(cur);
                cur = BookItem();
            }
            hasData = false;
            continue;
        }
        int pos = line.indexOf(':');
        if (pos > 0) {
            const QString key = trim(line.left(pos));
            const QString val = trim(line.mid(pos + 1));
            hasData = true;

            if      (key == "ISBN")       cur.isbn = val;
            else if (key == "Title")      cur.title = val;
            else if (key == "Author")     cur.author = val;
            else if (key == "Publisher")  cur.publisher = val;
            else if (key == "Date")       cur.date = val;
            else if (key == "Category")   cur.category = val;
            else if (key == "CallNumber") cur.callNumber = val;
            else if (key == "Total")      cur.total = val.toInt();
            else if (key == "Available")  cur.available = val.toInt();
        }
    }
    // 收尾：最后一条如果没有空行也要入表
    if (hasData && !cur.title.isEmpty()) items_.push_back(cur);

    loaded_ = true;
    return true;
}


QVector<BookItem> BookSearchEngine::searchByKeyword(const QString& keyword) const {
    QVector<BookItem> out;
    if (!loaded_) return out;
    const QString k = keyword.trimmed();
    if (k.isEmpty()) return out;

    for (const auto& b : items_) {
        if (containsCaseInsensitive(b.title, k) || containsCaseInsensitive(b.author, k)) {
            out.push_back(b);
        }
    }
    return out;
}

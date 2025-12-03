#pragma once
#include <QString>
#include <QVector>
#include <QHash>




// 与 books.txt 字段一一对应（键名按你的文件保持一致）
struct BookItem {
    QString isbn;
    QString title;
    QString author;
    QString publisher;
    QString date;
    QString category;
    QString callNumber;
    int     total = 0;
    int     available = 0;
};

class BookSearchEngine {
public:
    // 加载：传入 books.txt 的绝对或相对路径
    bool loadFromFile(const QString& filePath, QString* errMsg = nullptr);

    // 根据关键词检索（忽略大小写；命中 Title 或 Author 任意字段即返回）
    QVector<BookItem> searchByKeyword(const QString& keyword) const;

    // 是否已加载成功
    bool ready() const { return loaded_; }

private:
    static QString trim(const QString& s);
    void pushOneIfValid();

private:
    bool loaded_ = false;
    QVector<BookItem> items_;
};

#ifndef LOCALDATASOURCE_H
#define LOCALDATASOURCE_H

#include "DataSource.h"

#include <QSqlDatabase>
#include <QString>

// 真数据源:直接操作本机 SQLite 文件,供服务器进程使用。
// 打开后按表定义自动建表,所有写入都会发出 dataChanged 信号。
class LocalDataSource : public DataSource
{
    Q_OBJECT
public:
    explicit LocalDataSource(QObject *parent = nullptr);
    ~LocalDataSource() override;

    // 打开(不存在则创建)数据库文件,并按表定义自动建表
    bool open(const QString &dbPath);
    bool isOpen() const;
    QString dbPath() const;

    // ---- DataSource 接口 ----
    QueryResult query(const QString &table,
                      const QStringList &fields = {},
                      const QString &where = QString(),
                      const QVariantList &bindValues = {}) override;
    qlonglong insertRow(const QString &table,
                        const QHash<QString, QVariant> &values) override;
    int updateRows(const QString &table,
                   const QHash<QString, QVariant> &values,
                   const QString &where,
                   const QVariantList &bindValues = {}) override;
    int removeRows(const QString &table,
                   const QString &where,
                   const QVariantList &bindValues = {}) override;

private:
    bool createTablesIfNeeded();
    // 旧库补列:CREATE TABLE IF NOT EXISTS 只建表不补列,
    // 旧版本生成的库会缺新版新增的列(曾导致写入静默失败)。
    // 对照表定义,用 PRAGMA 找出缺失列并 ALTER TABLE ADD COLUMN 自动补齐。
    bool migrateMissingColumns();

    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_open = false;
};

#endif // LOCALDATASOURCE_H

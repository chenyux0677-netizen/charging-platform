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
    bool loginAdmin(const QString &username, const QString &password) override;
    DataRow loginUser(const QString &phone) override;
    bool rechargeBalance(qlonglong userId, double amount) override;
    qlonglong startCharge(qlonglong userId, qlonglong pileId) override;
    bool settleCharge(qlonglong orderId) override;
    bool removeChargingPile(qlonglong pileId) override;
    bool removeChargingStation(qlonglong stationId) override;

private:
    bool createTablesIfNeeded();
    bool recoverChargingState();

    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_open = false;
};

#endif // LOCALDATASOURCE_H

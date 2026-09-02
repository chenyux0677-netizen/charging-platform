#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

// 一行数据: 列名 -> 值
using DataRow = QHash<QString, QVariant>;
// 查询结果: 行列表
using QueryResult = QVector<DataRow>;

// 数据源抽象接口 —— 两端界面与服务器都只认这个接口,不管数据是本地还是网络来的。
// 具体实现:
//   LocalDataSource  : 真连 SQLite(服务器进程用)
//   RemoteDataSource : 走网络收发消息(用户端/管理员端界面用)
class DataSource : public QObject
{
    Q_OBJECT
public:
    explicit DataSource(QObject *parent = nullptr);

    // 通用查询:查 table 的 fields(空 = 全部),可带 where 条件与绑定参数
    virtual QueryResult query(const QString &table,
                              const QStringList &fields = {},
                              const QString &where = QString(),
                              const QVariantList &bindValues = {}) = 0;

    // 插入一行,返回新行 id;失败返回 -1
    virtual qlonglong insertRow(const QString &table,
                                const QHash<QString, QVariant> &values) = 0;

    // 更新行,返回受影响行数
    virtual int updateRows(const QString &table,
                           const QHash<QString, QVariant> &values,
                           const QString &where,
                           const QVariantList &bindValues = {}) = 0;

    // 删除行,返回受影响行数
    virtual int removeRows(const QString &table,
                           const QString &where,
                           const QVariantList &bindValues = {}) = 0;

    // ===== 业务级接口(在服务器端以单事务执行,保证并发与数据一致) =====

    // 用户开始充电:占用电桩并创建"充电中"订单。
    // 成功返回订单 id;电桩被占用 / 用户已有进行中订单 / 用户异常等失败返回 -1。
    virtual qlonglong startCharge(qlonglong userId, qlonglong pileId) = 0;

    // 结算订单:时长与费用由服务器计算,幂等(同一订单只会成功结算一次)。
    // 余额不足则整单回滚并返回 false(订单保持"充电中",充值后可重试)。
    virtual bool settleCharge(qlonglong orderId) = 0;

    // 充值:只在用户状态正常时把余额加上 amount。
    virtual bool rechargeBalance(qlonglong userId, double amount) = 0;

    // 删除电桩:仅当不在使用中且没有任何订单记录时成功(有历史订单则拒绝)。
    virtual bool removeChargingPile(qlonglong pileId) = 0;

    // 删除充电站:任一电桩使用中或有过订单记录则拒绝;否则在事务中先删桩再删站。
    virtual bool removeChargingStation(qlonglong stationId) = 0;

signals:
    // 任何成功写入(增/删/改)后发出,携带表名 —— 两端界面据此刷新
    void dataChanged(const QString &table);
};

#endif // DATASOURCE_H

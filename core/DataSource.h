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

    // 登录必须交给数据源/服务器完成，界面不能直接查询账号表。
    virtual bool loginAdmin(const QString &username, const QString &password) = 0;
    // 手机号不存在时原子注册并登录；失败或账号冻结返回空行。
    virtual DataRow loginUser(const QString &phone) = 0;

    // 余额只能通过服务端增量充值，不能由客户端直接覆盖。
    virtual bool rechargeBalance(qlonglong userId, double amount) = 0;

    // 核心充电业务必须由服务器原子执行，不能由界面拼接多次 CRUD。
    // startCharge 成功返回新订单 id，失败（用户已有订单/桩非空闲等）返回 -1。
    virtual qlonglong startCharge(qlonglong userId, qlonglong pileId) = 0;

    // 按服务器时间更新进行中订单的时长、电量和费用，供管理端实时查看。
    virtual bool updateChargingProgress(qlonglong orderId) = 0;

    // 停止充电：冻结费用、释放电桩并转为“待支付”，不因余额不足回滚。
    virtual bool stopCharge(qlonglong orderId) = 0;

    // 支付一笔“待支付”订单，只扣减已冻结的金额并标记“已完成”。
    virtual bool settleCharge(qlonglong orderId) = 0;

    // 管理端安全删除：有关联订单或正在使用时拒绝，避免产生孤儿数据。
    virtual bool removeChargingPile(qlonglong pileId) = 0;
    virtual bool removeChargingStation(qlonglong stationId) = 0;

signals:
    // 任何成功写入(增/删/改)后发出,携带表名 —— 两端界面据此刷新
    void dataChanged(const QString &table);
};

#endif // DATASOURCE_H

#ifndef REMOTEDATASOURCE_H
#define REMOTEDATASOURCE_H

#include "DataSource.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QTcpSocket>

#include <functional>

// 网络版数据源:实现与 LocalDataSource 相同的 DataSource 接口,
// 底层改为"把请求编码成 JSON 发给服务器、阻塞等回包"。
// 界面层使用它时,无需知道数据来自本地还是网络。
//
// 同步策略:每个请求发出去后,用一个嵌套事件循环阻塞等待对应 reqId 的响应;
// 服务器广播的 dataChanged 通知统一用 0ms 定时器推到事件循环下一轮再转成
// dataChanged 信号发出。绝不从 onReadyRead 内部同步发出——页面收到通知后要
// 做阻塞查询,若查询发生在 readyRead 处理栈内,回包期间的 readyRead 会被 Qt
// 屏蔽,查询必然超时。请求进行中收到的通知仍先暂存,待请求彻底结束再补发。
class RemoteDataSource : public DataSource
{
    Q_OBJECT
public:
    explicit RemoteDataSource(QObject *parent = nullptr);

    // 连接服务器(阻塞,超过 timeoutMs 毫秒未连上返回 false)
    bool connectToServer(const QString &host, quint16 port, int timeoutMs = 3000);
    bool isConnected() const;

    // ---- DataSource 接口(阻塞实现) ----
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
    bool updateChargingProgress(qlonglong orderId) override;
    bool stopCharge(qlonglong orderId) override;
    bool settleCharge(qlonglong orderId) override;
    bool restartChargingPile(qlonglong pileId) override;
    bool removeChargingPile(qlonglong pileId) override;
    bool removeChargingStation(qlonglong stationId) override;

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);

private slots:
    void onReadyRead();

private:
    QVariant sendRequestAndWait(const QJsonObject &request);
    // 把暂存的 dataChanged 通知统一发出(仅当没有请求在等待回包时)
    void flushDeferredNotifies();
    // 用 0ms 定时器把通知推到事件循环下一轮发出,绝不在 readyRead 处理栈内同步发
    void scheduleFlush();

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;               // 收包缓冲(按帧切分)
    quint32 m_nextReqId = 0;           // 自增请求号
    bool m_connected = false;
    QHash<quint32, std::function<void(const QVariant &)>> m_pending; // reqId -> 回调
    bool m_flushScheduled = false;     // 已有一次延迟通知排队,避免重复排队

    // 收到但暂未发出的 dataChanged 通知(等没有请求在等待回包时统一补发)
    QStringList m_deferredNotifies;
};

#endif // REMOTEDATASOURCE_H

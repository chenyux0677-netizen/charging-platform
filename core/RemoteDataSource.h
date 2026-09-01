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
// 服务器广播的 dataChanged 通知则在等待期间即时处理并转成 dataChanged 信号。
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

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);

private slots:
    void onReadyRead();

private:
    QVariant sendRequestAndWait(const QJsonObject &request);

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;               // 收包缓冲(按帧切分)
    quint32 m_nextReqId = 0;           // 自增请求号
    bool m_connected = false;
    QHash<quint32, std::function<void(const QVariant &)>> m_pending; // reqId -> 回调
};

#endif // REMOTEDATASOURCE_H

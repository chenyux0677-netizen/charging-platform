#ifndef SERVER_H
#define SERVER_H

#include "core/LocalDataSource.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>

// 数据库中间层(服务器进程):
//   监听 TCP → 收到客户端 JSON 请求 → 转给 LocalDataSource 执行 → 回响应;
//   数据被写入时,把 dataChanged 广播给所有在线客户端。
// 它本身不关心业务,只管"存取数据 + 通知大家数据变了"。
class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

    // 启动:打开(或创建)SQLite 并自动建表,监听指定地址/端口。
    // port 传 0 表示由系统自动分配空闲端口(可用 port() 查询实际端口)。
    bool start(const QString &dbPath, const QString &listenAddress, quint16 port);
    bool isRunning() const;
    quint16 port() const;

    // 首次启动写入基础数据(当前:默认管理员 admin / 123456)
    void seedInitialData();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    struct ClientSession {
        enum Role { None, User, Admin } role = None;
        qlonglong userId = 0;
        QString adminUsername;
    };

    void handleMessage(QTcpSocket *client, const QJsonObject &msg);
    void sendToClient(QTcpSocket *client, const QJsonObject &msg);
    void broadcastDataChanged(const QString &table);

    LocalDataSource *m_ds = nullptr;
    QTcpServer *m_server = nullptr;
    QSet<QTcpSocket *> m_clients;            // 在线客户端集合
    QHash<QTcpSocket *, QByteArray> m_buffers; // 每个客户端未解完的收包缓冲
    QHash<QTcpSocket *, ClientSession> m_sessions; // 身份与 TCP 连接绑定
};

#endif // SERVER_H

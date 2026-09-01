#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include "core/DataSource.h"

#include <QObject>
#include <QString>

class RemoteDataSource;

// 全局上下文:整个进程只有一份。
// 存放三类状态:
//   1. 服务器地址(用户端手动填;管理员端填本机 127.0.0.1)
//   2. 全局唯一的数据源 —— 两端界面一律经它访问数据,
//      底层是本地还是网络,对界面完全无感
//   3. 当前登录会话(用户行 / 管理员账号)
class AppContext : public QObject
{
    Q_OBJECT
public:
    static AppContext *instance();

    // ---- 服务器地址 ----
    void setServer(const QString &host, quint16 port);
    QString serverHost() const;
    quint16 serverPort() const;

    // ---- 全局数据源 ----
    // 先连接(首次或地址变更后),连上才返回 true
    bool connectIfNeeded(int timeoutMs = 3000);
    DataSource *dataSource();

    // ---- 登录会话 ----
    void setCurrentUser(const DataRow &user);
    DataRow currentUser() const;
    void setCurrentAdmin(const QString &username);
    QString currentAdmin() const;

private:
    explicit AppContext(QObject *parent = nullptr);
    Q_DISABLE_COPY(AppContext)

    QString m_host;
    quint16 m_port = 0;
    RemoteDataSource *m_ds = nullptr;
    bool m_connected = false;

    DataRow m_user;         // 当前登录用户(整行)
    QString m_admin;        // 当前登录管理员账号
};

#endif // APPCONTEXT_H

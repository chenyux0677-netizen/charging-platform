#include "AppContext.h"

#include "core/RemoteDataSource.h"

AppContext *AppContext::instance()
{
    static AppContext ctx;
    return &ctx;
}

AppContext::AppContext(QObject *parent)
    : QObject(parent)
{
    m_ds = new RemoteDataSource(this);
}

void AppContext::setServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_connected = false; // 换了地址,下次必须重连
}

QString AppContext::serverHost() const
{
    return m_host;
}

quint16 AppContext::serverPort() const
{
    return m_port;
}

bool AppContext::connectIfNeeded(int timeoutMs)
{
    if (m_connected && m_ds->isConnected())
        return true;
    if (m_host.isEmpty() || m_port == 0)
        return false;
    m_connected = m_ds->connectToServer(m_host, m_port, timeoutMs);
    return m_connected;
}

DataSource *AppContext::dataSource()
{
    return m_ds;
}

void AppContext::setCurrentUser(const DataRow &user)
{
    m_user = user;
}

DataRow AppContext::currentUser() const
{
    return m_user;
}

void AppContext::setCurrentAdmin(const QString &username)
{
    m_admin = username;
}

QString AppContext::currentAdmin() const
{
    return m_admin;
}

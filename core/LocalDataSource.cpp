#include "LocalDataSource.h"
#include "TableDef.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {
// 本机唯一的命名连接,避免重复注册报警
const QString kConnName = QStringLiteral("charging_local");
}

LocalDataSource::LocalDataSource(QObject *parent)
    : DataSource(parent)
{
}

LocalDataSource::~LocalDataSource()
{
    m_db = QSqlDatabase(); // 先释放内部引用
    QSqlDatabase::removeDatabase(kConnName);
}

bool LocalDataSource::open(const QString &dbPath)
{
    m_dbPath = dbPath;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "[LocalDataSource] 打开数据库失败:" << m_db.lastError().text();
        return false;
    }
    if (!createTablesIfNeeded()) {
        return false;
    }
    m_open = true;
    qInfo() << "[LocalDataSource] 数据库已就绪:" << dbPath;
    return true;
}

bool LocalDataSource::isOpen() const
{
    return m_open;
}

QString LocalDataSource::dbPath() const
{
    return m_dbPath;
}

bool LocalDataSource::createTablesIfNeeded()
{
    for (const TableDef &def : allTableDefs()) {
        QSqlQuery q(m_db);
        if (!q.exec(def.createTableSql())) {
            qWarning() << "[LocalDataSource] 建表失败:" << def.tableName
                       << "->" << q.lastError().text();
            return false;
        }
        qInfo() << "[LocalDataSource] 表已就绪:" << def.tableName;
    }
    return true;
}

QueryResult LocalDataSource::query(const QString &table,
                                   const QStringList &fields,
                                   const QString &where,
                                   const QVariantList &bindValues)
{
    QueryResult result;
    if (!m_db.isOpen())
        return result;

    const QString fieldSql = fields.isEmpty()
        ? QStringLiteral("*")
        : fields.join(QStringLiteral(", "));
    QString sql = QStringLiteral("SELECT %1 FROM %2").arg(fieldSql, table);
    if (!where.isEmpty())
        sql += QStringLiteral(" WHERE ") + where;

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 查询失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return result;
    }

    const QSqlRecord rec = q.record();
    while (q.next()) {
        DataRow row;
        for (int i = 0; i < rec.count(); ++i)
            row.insert(rec.fieldName(i), q.value(i));
        result << row;
    }
    return result;
}

qlonglong LocalDataSource::insertRow(const QString &table,
                                     const QHash<QString, QVariant> &values)
{
    if (!m_db.isOpen() || values.isEmpty())
        return -1;

    const QStringList cols = values.keys();
    const QString colSql = cols.join(QStringLiteral(", "));
    QStringList placeholders;
    for (int i = 0; i < cols.size(); ++i)
        placeholders << QStringLiteral("?");

    const QString sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
                            .arg(table, colSql, placeholders.join(QStringLiteral(", ")));

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QString &c : cols)
        q.addBindValue(values.value(c));

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 插入失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return -1;
    }

    const qlonglong newId = q.lastInsertId().toLongLong();
    emit dataChanged(table);
    return newId;
}

int LocalDataSource::updateRows(const QString &table,
                                const QHash<QString, QVariant> &values,
                                const QString &where,
                                const QVariantList &bindValues)
{
    if (!m_db.isOpen() || values.isEmpty() || where.isEmpty())
        return 0;

    QStringList sets;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sets << (it.key() + QStringLiteral(" = ?"));
    const QString sql = QStringLiteral("UPDATE %1 SET %2 WHERE %3")
                            .arg(table, sets.join(QStringLiteral(", ")), where);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        q.addBindValue(it.value());
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 更新失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return 0;
    }

    const int affected = q.numRowsAffected();
    emit dataChanged(table);
    return affected;
}

int LocalDataSource::removeRows(const QString &table,
                                const QString &where,
                                const QVariantList &bindValues)
{
    if (!m_db.isOpen() || where.isEmpty())
        return 0;

    const QString sql = QStringLiteral("DELETE FROM %1 WHERE %2").arg(table, where);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 删除失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return 0;
    }

    const int affected = q.numRowsAffected();
    emit dataChanged(table);
    return affected;
}

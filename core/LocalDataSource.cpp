#include "LocalDataSource.h"
#include "TableDef.h"

#include <QDebug>
#include <QSet>
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
    // CREATE 只保证表存在;旧版本生成的库会缺新版新增的列,这里自动补齐(见 migrateMissingColumns)
    return migrateMissingColumns();
}

bool LocalDataSource::migrateMissingColumns()
{
    for (const TableDef &def : allTableDefs()) {
        // 读出表现有的列名集合
        QSet<QString> existing;
        {
            QSqlQuery qCol(m_db);
            if (!qCol.exec(QStringLiteral("PRAGMA table_info(%1)").arg(def.tableName))) {
                qWarning() << "[LocalDataSource] 读取列信息失败:" << def.tableName
                           << "->" << qCol.lastError().text();
                return false;
            }
            // PRAGMA table_info 每行: [cid, name, type, notnull, dflt_value, pk]
            while (qCol.next())
                existing.insert(qCol.value(1).toString());
        }

        for (const ColumnDef &col : def.columns) {
            if (existing.contains(col.name))
                continue;

            // SQLite 的 ALTER TABLE ADD COLUMN 不能带主键/自增/唯一约束
            if (col.primaryKey || col.autoIncrement || col.unique) {
                qWarning() << "[LocalDataSource] 列" << def.tableName << "." << col.name
                           << "带主键/自增/唯一约束,无法自动补充,需删旧库重建(当前代码不会新增此类列)";
                continue;
            }

            // ADD COLUMN 的 DEFAULT 必须是字面量,带括号的表达式(如 datetime(...))不被允许
            const bool constantDefault = !col.defaultValue.isEmpty()
                && !col.defaultValue.startsWith(QLatin1Char('('));

            QString ddl = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                              .arg(def.tableName, col.name, col.sqlType);
            if (col.notNull && constantDefault) {
                ddl += QStringLiteral(" NOT NULL DEFAULT %1").arg(col.defaultValue);
            } else if (!col.notNull && constantDefault) {
                ddl += QStringLiteral(" DEFAULT %1").arg(col.defaultValue);
            } else if (col.notNull) {
                // 要求 NOT NULL 但拿不到字面量默认值:SQLite 不允许,降级为可空列
                // (旧行与依赖数据库默认值的新行此列可能为 NULL,但不影响读写正常)
                qWarning() << "[LocalDataSource] 列" << def.tableName << "." << col.name
                           << "要求 NOT NULL 但默认值非常量,已按可空列补充";
            }

            QSqlQuery qAdd(m_db);
            if (!qAdd.exec(ddl)) {
                qWarning() << "[LocalDataSource] 自动补列失败:" << def.tableName << "." << col.name
                           << "->" << qAdd.lastError().text() << "| SQL:" << ddl;
                return false;
            }
            qInfo() << "[LocalDataSource] 旧库自动补列:" << def.tableName << "+" << col.name;
        }
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

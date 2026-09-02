#ifndef TABLEDEF_H
#define TABLEDEF_H

#include <QString>
#include <QVector>

// 一列的字段定义
struct ColumnDef
{
    QString name;           // 列名
    QString sqlType;        // SQL 类型: INTEGER / TEXT / REAL
    bool primaryKey = false;    // 是否主键
    bool autoIncrement = false; // 是否自增(通常配合主键)
    bool notNull = false;       // 是否 NOT NULL
    bool unique = false;        // 是否 UNIQUE
    QString defaultValue;       // DEFAULT 值,空表示不写

    ColumnDef() = default;
    ColumnDef(const QString &n, const QString &t,
              bool pk = false, bool ai = false,
              bool nn = false, bool uniq = false,
              const QString &dv = QString())
        : name(n), sqlType(t), primaryKey(pk), autoIncrement(ai),
          notNull(nn), unique(uniq), defaultValue(dv) {}
};

// 一张表的定义
struct TableDef
{
    QString tableName;              // 表名
    QVector<ColumnDef> columns;     // 字段清单
    // 表级约束(CHECK / FOREIGN KEY / UNIQUE 等),创建时拼在列清单后面;仅对新建库生效
    QVector<QString> constraints;
    // 建表完成后要执行的 CREATE INDEX 语句(如订单的部分唯一索引);启动时逐条执行
    QVector<QString> indexes;

    // 生成 CREATE TABLE IF NOT EXISTS 语句
    QString createTableSql() const;
};

// 全项目业务表注册表:
// ★ 数据要变动(加字段/改字段),只改这一个函数,框架代码不用动
QVector<TableDef> allTableDefs();

#endif // TABLEDEF_H

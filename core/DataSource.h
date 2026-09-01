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

signals:
    // 任何成功写入(增/删/改)后发出,携带表名 —— 两端界面据此刷新
    void dataChanged(const QString &table);
};

#endif // DATASOURCE_H

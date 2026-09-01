#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

// 两端通信协议(JSON + 长度前缀帧),客户端与服务器共用这份"契约"。
//
//   请求    kind=req    { reqId, op, table, [fields], [where], [bind], [values] }
//   响应    kind=resp   { reqId, ok, data, [error] }
//   广播    kind=notify { event: "dataChanged", table }
//
// 帧格式: [4 字节大端长度][JSON 字节]
namespace Protocol {

// 构造请求。op 取值: query / insert / update / remove
QJsonObject makeRequest(quint32 reqId, const QString &op, const QString &table,
                        const QStringList &fields = {},
                        const QString &where = QString(),
                        const QVariantList &bind = {},
                        const QHash<QString, QVariant> &values = {});

// 构造 dataChanged 广播
QJsonObject makeDataChanged(const QString &table);

// QVariant <-> QJsonValue 转换(JSON 无法表达的原子类型统一按字符串处理)
QJsonValue variantToJson(const QVariant &v);
QVariant jsonToVariant(const QJsonValue &v);
QHash<QString, QVariant> jsonToValues(const QJsonObject &obj);

// 帧编解码
QByteArray encodeFrame(const QJsonObject &msg);
// 从累积缓冲中取一个完整帧;成功返回 true 并写入 outMsg。
// 数据不足或解析失败返回 false(数据不足时缓冲保留,等更多字节到达)。
bool tryDecodeFrame(QByteArray &buffer, QJsonObject &outMsg);

} // namespace Protocol

#endif // PROTOCOL_H

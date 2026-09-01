#include "Protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaType>

namespace Protocol {

QJsonObject makeRequest(quint32 reqId, const QString &op, const QString &table,
                        const QStringList &fields, const QString &where,
                        const QVariantList &bind, const QHash<QString, QVariant> &values)
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("req"));
    o.insert(QStringLiteral("reqId"), QJsonValue(static_cast<qint64>(reqId)));
    o.insert(QStringLiteral("op"), op);
    o.insert(QStringLiteral("table"), table);

    if (!fields.isEmpty())
        o.insert(QStringLiteral("fields"), QJsonArray::fromStringList(fields));
    if (!where.isEmpty())
        o.insert(QStringLiteral("where"), where);
    if (!bind.isEmpty()) {
        QJsonArray b;
        for (const QVariant &v : bind)
            b.append(variantToJson(v));
        o.insert(QStringLiteral("bind"), b);
    }
    if (!values.isEmpty()) {
        QJsonObject vals;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            vals.insert(it.key(), variantToJson(it.value()));
        o.insert(QStringLiteral("values"), vals);
    }
    return o;
}

QJsonObject makeDataChanged(const QString &table)
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("notify"));
    o.insert(QStringLiteral("event"), QStringLiteral("dataChanged"));
    o.insert(QStringLiteral("table"), table);
    return o;
}

QJsonValue variantToJson(const QVariant &v)
{
    switch (v.typeId()) {
    case QMetaType::Int:
        return QJsonValue(v.toInt());
    case QMetaType::LongLong:
        return QJsonValue(v.toLongLong());
    case QMetaType::ULongLong:
        return QJsonValue(static_cast<qint64>(v.toULongLong()));
    case QMetaType::Double:
        return QJsonValue(v.toDouble());
    case QMetaType::Bool:
        return QJsonValue(v.toBool());
    case QMetaType::QString:
        return QJsonValue(v.toString());
    default:
        // 其它类型(BLOB、日期时间等)统一按字符串表达,保证 JSON 可序列化
        return QJsonValue(v.toString());
    }
}

QVariant jsonToVariant(const QJsonValue &v)
{
    return v.toVariant();
}

QHash<QString, QVariant> jsonToValues(const QJsonObject &obj)
{
    QHash<QString, QVariant> values;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        values.insert(it.key(), jsonToVariant(it.value()));
    return values;
}

QByteArray encodeFrame(const QJsonObject &msg)
{
    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    const quint32 len = static_cast<quint32>(payload.size());

    QByteArray frame;
    frame.reserve(4 + payload.size());
    frame.append(static_cast<char>((len >> 24) & 0xFF));
    frame.append(static_cast<char>((len >> 16) & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(payload);
    return frame;
}

bool tryDecodeFrame(QByteArray &buffer, QJsonObject &outMsg)
{
    if (buffer.size() < 4)
        return false;

    const quint32 len = (static_cast<quint32>(static_cast<uchar>(buffer[0])) << 24)
                      | (static_cast<quint32>(static_cast<uchar>(buffer[1])) << 16)
                      | (static_cast<quint32>(static_cast<uchar>(buffer[2])) << 8)
                      | static_cast<quint32>(static_cast<uchar>(buffer[3]));
    if (buffer.size() < 4 + static_cast<int>(len))
        return false; // 帧未收全,保留缓冲等更多数据

    const QByteArray payload = buffer.mid(4, static_cast<int>(len));
    buffer.remove(0, 4 + static_cast<int>(len));

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError)
        return false;
    outMsg = doc.object();
    return true;
}

} // namespace Protocol

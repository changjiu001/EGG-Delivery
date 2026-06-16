#include "wire_protocol.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>

QByteArray WireProtocol::encode(const WireMessage &message)
{
    const QByteArray headerBytes = QJsonDocument(message.header).toJson(QJsonDocument::Compact);

    QByteArray body;
    QDataStream bodyStream(&body, QIODevice::WriteOnly);
    bodyStream.setByteOrder(QDataStream::BigEndian);
    bodyStream << WIRE_MAGIC;
    bodyStream << WIRE_VERSION;
    bodyStream << static_cast<quint16>(message.type);
    bodyStream << static_cast<quint32>(headerBytes.size());
    bodyStream << static_cast<quint32>(message.payload.size());
    body.append(headerBytes);
    body.append(message.payload);

    QByteArray frame;
    QDataStream frameStream(&frame, QIODevice::WriteOnly);
    frameStream.setByteOrder(QDataStream::BigEndian);
    frameStream << static_cast<quint32>(body.size());
    frame.append(body);
    return frame;
}

QList<WireMessage> WireProtocol::takeMessages(QByteArray &buffer, QString *error)
{
    QList<WireMessage> messages;

    while (buffer.size() >= static_cast<int>(sizeof(quint32))) {
        QDataStream sizeStream(buffer.left(sizeof(quint32)));
        sizeStream.setByteOrder(QDataStream::BigEndian);

        quint32 frameSize = 0;
        sizeStream >> frameSize;

        if (frameSize == 0 || frameSize > MAX_FRAME_SIZE) {
            if (error) {
                *error = QStringLiteral("非法协议帧长度：%1").arg(frameSize);
            }
            buffer.clear();
            break;
        }

        if (buffer.size() < static_cast<int>(sizeof(quint32) + frameSize)) {
            break;
        }

        const QByteArray body = buffer.mid(sizeof(quint32), frameSize);
        buffer.remove(0, sizeof(quint32) + frameSize);

        QDataStream stream(body);
        stream.setByteOrder(QDataStream::BigEndian);

        quint32 magic = 0;
        quint16 version = 0;
        quint16 type = 0;
        quint32 headerLen = 0;
        quint32 payloadLen = 0;

        stream >> magic >> version >> type >> headerLen >> payloadLen;

        const int prefixSize = sizeof(quint32) + sizeof(quint16) + sizeof(quint16)
                             + sizeof(quint32) + sizeof(quint32);

        if (magic != WIRE_MAGIC || version != WIRE_VERSION) {
            if (error) {
                *error = QStringLiteral("协议魔数或版本不匹配");
            }
            continue;
        }

        if (prefixSize + static_cast<int>(headerLen) + static_cast<int>(payloadLen) != body.size()) {
            if (error) {
                *error = QStringLiteral("协议帧长度不一致");
            }
            continue;
        }

        const QByteArray headerBytes = body.mid(prefixSize, headerLen);
        const QByteArray payload = body.mid(prefixSize + headerLen, payloadLen);
        const QJsonDocument doc = QJsonDocument::fromJson(headerBytes);

        if (!doc.isObject()) {
            if (error) {
                *error = QStringLiteral("协议头不是合法 JSON 对象");
            }
            continue;
        }

        WireMessage message;
        message.type = static_cast<WireMessageType>(type);
        message.header = doc.object();
        message.payload = payload;
        messages.append(message);
    }

    return messages;
}

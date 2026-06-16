#ifndef WIRE_PROTOCOL_H
#define WIRE_PROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

constexpr quint32 WIRE_MAGIC = 0x514C414E;
constexpr quint16 WIRE_VERSION = 1;
constexpr int MAX_FRAME_SIZE = 256 * 1024 * 1024;

enum class WireMessageType : quint16 {
    Text = 1,
    Sticker = 2,
    FileStart = 16,
    FileAccept = 17,
    FileReject = 18,
    FileChunk = 19,
    FileComplete = 20,
    FileCancel = 21,
    Error = 100
};

struct WireMessage {
    WireMessageType type = WireMessageType::Error;
    QJsonObject header;
    QByteArray payload;
};

class WireProtocol {
public:
    static QByteArray encode(const WireMessage &message);
    static QList<WireMessage> takeMessages(QByteArray &buffer, QString *error = nullptr);
};

#endif // WIRE_PROTOCOL_H

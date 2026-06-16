#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <QDateTime>
#include <QHostAddress>
#include <QMetaType>
#include <QString>

struct PeerInfo {
    QString peerId;
    QString name;
    QHostAddress address;
    quint16 tcpPort = 0;
    qint64 lastSeenMs = 0;
};

struct FileOfferInfo {
    QString transferId;
    QString fromPeerId;
    QString fromPeerName;
    QString fileName;
    qint64 fileSize = 0;
    quint32 totalChunks = 0;
};

enum class TransferDirection {
    Upload,
    Download
};

enum class TransferState {
    Waiting,
    Transferring,
    Finished,
    Rejected,
    Failed,
    Cancelled
};

struct TransferView {
    QString transferId;
    QString peerId;
    QString peerName;
    QString fileName;
    QString localPath;
    qint64 fileSize = 0;
    int progress = 0;
    TransferDirection direction = TransferDirection::Upload;
    TransferState state = TransferState::Waiting;
    QString message;
};

Q_DECLARE_METATYPE(PeerInfo)
Q_DECLARE_METATYPE(FileOfferInfo)
Q_DECLARE_METATYPE(TransferView)

inline QString shortPeerName(const PeerInfo &peer)
{
    return peer.name.isEmpty() ? peer.peerId.left(8) : peer.name;
}

inline QString formatBytes(qint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0
        ? QString::number(bytes) + " B"
        : QString::number(value, 'f', 2) + ' ' + units[unit];
}

inline QString stateText(TransferState state)
{
    switch (state) {
    case TransferState::Waiting: return QStringLiteral("等待确认");
    case TransferState::Transferring: return QStringLiteral("传输中");
    case TransferState::Finished: return QStringLiteral("已完成");
    case TransferState::Rejected: return QStringLiteral("已拒绝");
    case TransferState::Failed: return QStringLiteral("失败");
    case TransferState::Cancelled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

#endif // APP_TYPES_H

#ifndef LAN_NETWORK_CLIENT_H
#define LAN_NETWORK_CLIENT_H

#include "app_types.h"
#include "tcp_connection.h"
#include "file_manager.h"

#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QTcpServer>
#include <QUdpSocket>
#include <vector>

class LanNetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit LanNetworkClient(QObject *parent = nullptr);
    ~LanNetworkClient() override;

    bool isRunning() const;
    QString localPeerId() const;
    QString downloadDirectory() const;
    QList<PeerInfo> peers() const;

public slots:
    bool start(const QString &userName, quint16 tcpPort, const QString &downloadDir);
    void stop();

    void sendText(const QString &peerId, const QString &text);
    void sendSticker(const QString &peerId, const QString &stickerId);
    void sendFile(const QString &peerId, const QString &filePath);
    void sendFolder(const QString &peerId, const QString &folderPath);
    void acceptFile(const QString &transferId);
    void rejectFile(const QString &transferId);
    void cancelTransfer(const QString &transferId);
    void setDownloadDirectory(const QString &dir);
    void addManualPeer(const QString &displayName, const QString &address, quint16 tcpPort);

signals:
    void started(quint16 tcpPort);
    void stopped();
    void peerOnline(const PeerInfo &peer);
    void peerOffline(const QString &peerId);

    void textReceived(const QString &peerId, const QString &peerName, const QString &text, qint64 timestampMs);
    void stickerReceived(const QString &peerId, const QString &peerName, const QString &stickerId, qint64 timestampMs);
    void textSendFailed(const QString &peerId, const QString &reason);

    void fileOfferReceived(const FileOfferInfo &offer);
    void transferCreated(const TransferView &transfer);
    void transferUpdated(const TransferView &transfer);
    void transferRemoved(const QString &transferId);
    void errorOccurred(const QString &message);

private slots:
    void broadcastHello();
    void readDiscoveryDatagrams();
    void expirePeers();
    void onNewTcpConnection();
    void onTcpMessage(TcpConnection *connection, const WireMessage &message);
    void onTcpDisconnected(TcpConnection *connection);
    void onTcpError(TcpConnection *connection, const QString &message);
    void sendPendingChunks();

private:
    struct OutgoingTransfer {
        TransferView view;
        std::vector<FileChunk> chunks;
        int nextChunk = 0;
        bool accepted = false;
        QString tempZipPath;
    };

    struct PendingIncomingOffer {
        FileOfferInfo offer;
    };

    void registerMetaTypes();
    void updatePeer(const PeerInfo &peer);
    void removeConnection(TcpConnection *connection);
    TcpConnection *connectionForPeer(const QString &peerId);
    void bindConnectionToPeer(TcpConnection *connection, const QString &peerId);
    bool sendToPeer(const QString &peerId, const WireMessage &message);

    WireMessage makeMessage(WireMessageType type, QJsonObject header = {}, QByteArray payload = {}) const;
    void handleText(const WireMessage &message);
    void handleSticker(const WireMessage &message);
    void handleFileStart(const WireMessage &message);
    void handleFileAccept(const WireMessage &message);
    void handleFileReject(const WireMessage &message);
    void handleFileChunk(const WireMessage &message);
    void handleFileComplete(const WireMessage &message);
    void handleFileCancel(const WireMessage &message);

    QString uniqueDownloadFileName(const QString &fileName) const;
    QString safeFileName(const QString &fileName) const;
    QString peerName(const QString &peerId) const;
    void emitTransfer(const TransferView &view);
    void cleanupOutgoingTempFile(const OutgoingTransfer &transfer);

    static constexpr quint16 DiscoveryPort = 8888;
    static constexpr int HelloIntervalMs = 2000;
    static constexpr int PeerExpireMs = 9000;

    QUdpSocket m_udpSocket;
    QTcpServer m_tcpServer;
    QTimer m_helloTimer;
    QTimer m_expireTimer;
    QTimer m_sendTimer;

    QString m_localPeerId;
    QString m_localName;
    quint16 m_tcpPort = 0;
    bool m_running = false;

    FileManager m_fileManager;
    QHash<QString, PeerInfo> m_peers;
    QHash<QString, TcpConnection *> m_connectionsByPeer;
    QHash<TcpConnection *, QString> m_peerByConnection;
    QHash<QString, OutgoingTransfer> m_outgoingTransfers;
    QHash<QString, PendingIncomingOffer> m_pendingIncomingOffers;
    QHash<QString, TransferView> m_transferViews;
};

#endif // LAN_NETWORK_CLIENT_H

#include "lan_network_client.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUuid>

namespace {

bool isLocalHostAddress(const QHostAddress &address)
{
    if (address.isLoopback()) {
        return true;
    }

    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &localAddress : addresses) {
        if (localAddress.protocol() == QAbstractSocket::IPv4Protocol && localAddress == address) {
            return true;
        }
    }
    return false;
}

QHostAddress normalizedPeerAddress(const QHostAddress &address)
{
    return isLocalHostAddress(address) ? QHostAddress::LocalHost : address;
}

} // namespace

LanNetworkClient::LanNetworkClient(QObject *parent)
    : QObject(parent), m_fileManager("./downloads")
{
    registerMetaTypes();

    connect(&m_helloTimer, &QTimer::timeout, this, &LanNetworkClient::broadcastHello);
    connect(&m_expireTimer, &QTimer::timeout, this, &LanNetworkClient::expirePeers);
    connect(&m_sendTimer, &QTimer::timeout, this, &LanNetworkClient::sendPendingChunks);
    connect(&m_udpSocket, &QUdpSocket::readyRead, this, &LanNetworkClient::readDiscoveryDatagrams);
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &LanNetworkClient::onNewTcpConnection);
}

LanNetworkClient::~LanNetworkClient()
{
    stop();
}

bool LanNetworkClient::isRunning() const
{
    return m_running;
}

QString LanNetworkClient::localPeerId() const
{
    return m_localPeerId;
}

QString LanNetworkClient::downloadDirectory() const
{
    return QString::fromStdString(m_fileManager.getDownloadDirectory());
}

QList<PeerInfo> LanNetworkClient::peers() const
{
    return m_peers.values();
}

bool LanNetworkClient::start(const QString &userName, quint16 tcpPort, const QString &downloadDir)
{
    if (m_running) {
        return true;
    }

    m_localName = userName.trimmed().isEmpty() ? QStringLiteral("匿名用户") : userName.trimmed();
    m_localPeerId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (!downloadDir.isEmpty()) {
        setDownloadDirectory(downloadDir);
    }

    if (!m_tcpServer.listen(QHostAddress::AnyIPv4, tcpPort)) {
        emit errorOccurred(QStringLiteral("TCP 监听失败：%1").arg(m_tcpServer.errorString()));
        return false;
    }
    m_tcpPort = m_tcpServer.serverPort();

    const bool udpOk = m_udpSocket.bind(QHostAddress::AnyIPv4,
                                        DiscoveryPort,
                                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!udpOk) {
        m_tcpServer.close();
        emit errorOccurred(QStringLiteral("UDP 发现端口绑定失败：%1").arg(m_udpSocket.errorString()));
        return false;
    }

    m_running = true;
    m_helloTimer.start(HelloIntervalMs);
    m_expireTimer.start(1000);
    broadcastHello();

    emit started(m_tcpPort);
    return true;
}

void LanNetworkClient::stop()
{
    if (!m_running) {
        return;
    }

    m_helloTimer.stop();
    m_expireTimer.stop();
    m_sendTimer.stop();
    m_udpSocket.close();
    m_tcpServer.close();

    const QList<TcpConnection *> connections = m_peerByConnection.keys();
    for (TcpConnection *connection : connections) {
        connection->close();
        connection->deleteLater();
    }

    m_connectionsByPeer.clear();
    m_peerByConnection.clear();
    m_peers.clear();
    m_outgoingTransfers.clear();
    m_pendingIncomingOffers.clear();
    m_transferViews.clear();

    m_running = false;
    emit stopped();
}

void LanNetworkClient::sendText(const QString &peerId, const QString &text)
{
    if (!m_running || text.trimmed().isEmpty()) {
        return;
    }

    QJsonObject header;
    header["messageId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    header["text"] = text;
    header["timestampMs"] = QString::number(QDateTime::currentMSecsSinceEpoch());

    if (!sendToPeer(peerId, makeMessage(WireMessageType::Text, header))) {
        emit textSendFailed(peerId, QStringLiteral("目标用户不可用"));
    }
}

void LanNetworkClient::sendSticker(const QString &peerId, const QString &stickerId)
{
    if (!m_running || peerId.isEmpty() || stickerId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject header;
    header["messageId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    header["stickerId"] = stickerId.trimmed();
    header["timestampMs"] = QString::number(QDateTime::currentMSecsSinceEpoch());

    if (!sendToPeer(peerId, makeMessage(WireMessageType::Sticker, header))) {
        emit errorOccurrred(QStringLiteral("发送行情包失败：百标用户不可用"));
    }
}

void LanNetworkClient::sendFile(const QString &peerId, const QString &filePath)
{
    if (!m_running) {
        emit errorOccurred(QStringLiteral("请先启动局域网客户端不昇));
        return;
    }

    if (!m_peers.contains(peerId)) {
        emit errorOccurred(QStringLiteral("未找到目标用户"));
        return;
    }

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        emit errorOccurred(QStringLiteral("文件不存在：%1").arg(filePath));
        return;
    }

    auto chunks = m_fileManager.readAndChunkFile(filePath.toStdString());
    if (chunks.empty()) {
        emit errorOccurred(QStringLiteral("文件分块失败：%1").arg(filePath));
        return;
    }

    const QString transferId = QString::fromStdString(chunks.front().file_hash);
    if (m_outgoingTransfers.contains(transferId)) {
        emit errorOccurred(QStringLiteral("该文件正在传输中"));
        return;
    }

    TransferView view;
    view.transferId = transferId;
    view.peerId = peerId;
    view.peerName = peerName(peerId);
    view.fileName = info.fileName();
    view.localPath = info.absoluteFilePath();
    view.fileSize = info.size();
    view.direction = TransferDirection::Upload;
    view.state = TransferState::Waiting;
    view.progress = 0;
    view.message = QStringLiteral("等待对方接收");


    if (!peerId.isEmpty() && m_connectionsByPeer.value(peerId) == connection) {
        m_connectionsByPeer.remove(peerId);
    }
}

TcpConnection *LanNetworkClient::connectionForPeer(const QString &peerId)
{
    if (m_connectionsByPeer.contains(peerId)) {
        TcpConnection *connection = m_connectionsByPeer.value(peerId);
        if (connection->state() != QAbstractSocket::UnconnectedState) {
            return connection;
        }
        removeConnection(connection);
        connection->deleteLater();
    }

    if (!m_peers.contains(peerId)) {
        return nullptr;
    }

    const PeerInfo peer = m_peers.value(peerId);
    auto *socket = new QTcpSocket(this);
    auto *connection = new TcpConnection(socket, this);
    connection->setPeerId(peerId);

    connect(connection, &TcpConnection::messageReceived, this, &LanNetworkClient::onTcpMessage);
    connect(connection, &TcpConnection::disconnected, this, &LanNetworkClient::onTcpDisconnected);
    connect(connection, &TcpConnection::errorOccurred, this, &LanNetworkClient::onTcpError);

    m_connectionsByPeer.insert(peerId, connection);
    m_peerByConnection.insert(connection, peerId);
    connection->connectToHost(peer.address, peer.tcpPort);
    return connection;
}

void LanNetworkClient::bindConnectionToPeer(TcpConnection *connection, const QString &peerId)
{
    if (peerId.isEmpty()) {
        return;
    }

    connection->setPeerId(peerId);
    m_peerByConnection.insert(connection, peerId);

    if (!m_connectionsByPeer.contains(peerId)) {
        m_connectionsByPeer.insert(peerId, connection);
    }
}

bool LanNetworkClient::sendToPeer(const QString &peerId, const WireMessage &message)
{
    TcpConnection *connection = connectionForPeer(peerId);
    if (!connection) {
        return false;
    }
    connection->send(message);
    return true;
}

WireMessage LanNetworkClient::makeMessage(WireMessageType type, QJsonObject header, QByteArray payload) const
{
    header["fromPeerId"] = m_localPeerId;
    header["fromPeerName"] = m_localName;
    header["timestampMs"] = QString::number(QDateTime::currentMSecsSinceEpoch());

    WireMessage message;
    message.type = type;
    message.header = header;
    message.payload = std::move(payload);
    return message;
}

void LanNetworkClient::handleText(const WireMessage &message)
{
    const QString fromPeerId = message.header.value("fromPeerId").toString();
    const QString fromPeerName = message.header.value("fromPeerName").toString(peerName(fromPeerId));
    const QString text = message.header.value("text").toString();
    const qint64 timestamp = message.header.value("timestampMs").toString().toLongLong();

    if (!fromPeerId.isEmpty() && !text.isEmpty()) {
        emit textReceived(fromPeerId, fromPeerName, text, timestamp);
    }
}

void LanNetworkClient::handleSticker(const WireMessage &message)
{
    const QString fromPeerId = message.header.value("fromPeerId").toString();
    const QString fromPeerName = message.header.value("fromPeerName").toString(peerName(fromPeerId));
    const QString stickerId = message.header.value("stickerId").toString();
    const qint64 timestamp = message.header.value("timestampMs").toString().toLongLong();

    if (!fromPeerId.isEmpty() && !stickerId.isEmpty()) {
        emit stickerReceived(fromPeerId, fromPeerName, stickerId, timestamp);
    }
}

void LanNetworkClient::handleFileStart(const WireMessage &message)
{
    const QString fromPeerId = message.header.value("fromPeerId").toString();
    const QString fromPeerName = message.header.value("fromPeerName").toString(peerName(fromPeerId));
    const QString transferId = message.header.value("transferId").toString();
    const QString fileName = safeFileName(message.header.value("fileName").toString());
    const qint64 fileSize = message.header.value("fileSize").toString().toLongLong();
    const quint32 totalChunks = static_cast<quint32>(message.header.value("totalChunks").toInt());

    if (fromPeerId.isEmpty() || transferId.isEmpty() || fileName.isEmpty() || totalChunks == 0) {
        emit errorOccurred(QStringLiteral("收到非法文件请求"));
        return;
    }

    FileOfferInfo offer;
    offer.transferId = transferId;
    offer.fromPeerId = fromPeerId;
    offer.fromPeerName = fromPeerName;
    offer.fileName = fileName;
    offer.fileSize = fileSize;
    offer.totalChunks = totalChunks;

    m_pendingIncomingOffers.insert(transferId, PendingIncomingOffer{offer});
    emit fileOfferReceived(offer);
}

void LanNetworkClient::handleFileAccept(const WireMessage &message)
{
    const QString transferId = message.header.value("transferId").toString();
    if (!m_outgoingTransfers.contains(transferId)) {
        return;
    }

    OutgoingTransfer &transfer = m_outgoingTransfers[transferId];
    transfer.accepted = true;
    transfer.view.state = TransferState::Transferring;
    transfer.view.message = QStringLiteral("对方已接收，开始发送");
    emitTransfer(transfer.view);

    if (!m_sendTimer.isActive()) {
        m_sendTimer.start(5);
    }
}

void LanNetworkClient::handleFileReject(const WireMessage &message)
{
    const QString transferId = message.header.value("transferId").toString();
    if (!m_outgoingTransfers.contains(transferId)) {
        return;
    }

    OutgoingTransfer transfer = m_outgoingTransfers.take(transferId);
    cleanupOutgoingTempFile(transfer);
    transfer.view.state = TransferState::Rejected;
    transfer.view.message = QStringLiteral("对方拒绝接收");
    emitTransfer(transfer.view);
}

void LanNetworkClient::handleFileChunk(const WireMessage &message)
{
    const QString transferId = message.header.value("transferId").toString();
    if (transferId.isEmpty() || !m_transferViews.contains(transferId)) {
        return;
    }

    const std::vector<uint8_t> wire(message.payload.begin(), message.payload.end());
    FileChunk chunk;
    if (!chunk.deserialize(wire)) {
        emit errorOccurred(QStringLiteral("收到损坏的文件分块"));
        return;
    }

    const QString chunkHash = QString::fromStdString(chunk.file_hash);
    if (chunkHash != transferId) {
        emit errorOccurred(QStringLiteral("文件分块 hash 与会话不一致"));
        return;

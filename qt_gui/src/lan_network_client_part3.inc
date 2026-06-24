        const QString peerId = obj.value("peerId").toString();
        if (peerId.isEmpty() || peerId == m_localPeerId) {
            continue;
        }

        const int tcpPort = obj.value("tcpPort").toInt();
        if (tcpPort <= 0 || tcpPort > 65535) {
            continue;
        }

        PeerInfo peer;
        peer.peerId = peerId;
        peer.name = obj.value("name").toString(QStringLiteral("匿名用户"));
        peer.address = normalizedPeerAddress(sender);
        peer.tcpPort = static_cast<quint16>(tcpPort);
        peer.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        updatePeer(peer);
    }
}

void LanNetworkClient::expirePeers()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> expired;

    for (auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it) {
        if (!it.key().startsWith(QStringLiteral("manual:"))
            && now - it.value().lastSeenMs > PeerExpireMs) {
            expired.append(it.key());
        }
    }

    for (const QString &peerId : expired) {
        m_peers.remove(peerId);
        if (m_connectionsByPeer.contains(peerId)) {
            m_connectionsByPeer.take(peerId)->close();
        }
        emit peerOffline(peerId);
    }

    m_fileManager.cleanupIdleSessions(3600);
}

void LanNetworkClient::onNewTcpConnection()
{
    while (m_tcpServer.hasPendingConnections()) {
        auto *connection = new TcpConnection(m_tcpServer.nextPendingConnection(), this);
        connect(connection, &TcpConnection::messageReceived, this, &LanNetworkClient::onTcpMessage);
        connect(connection, &TcpConnection::disconnected, this, &LanNetworkClient::onTcpDisconnected);
        connect(connection, &TcpConnection::errorOccurred, this, &LanNetworkClient::onTcpError);
        m_peerByConnection.insert(connection, QString());
    }
}

void LanNetworkClient::onTcpMessage(TcpConnection *connection, const WireMessage &message)
{
    const QString fromPeerId = message.header.value("fromPeerId").toString();
    if (!fromPeerId.isEmpty()) {
        bindConnectionToPeer(connection, fromPeerId);
    }

    switch (message.type) {
    case WireMessageType::Text:
        handleText(message);
        break;
    case WireMessageType::Sticker:
        handleSticker(message);
        break;
    case WireMessageType::FileStart:
        handleFileStart(message);
        break;
    case WireMessageType::FileAccept:
        handleFileAccept(message);
        break;
    case WireMessageType::FileReject:
        handleFileReject(message);
        break;
    case WireMessageType::FileChunk:
        handleFileChunk(message);
        break;
    case WireMessageType::FileComplete:
        handleFileComplete(message);
        break;
    case WireMessageType::FileCancel:
        handleFileCancel(message);
        break;
    case WireMessageType::Error:
        emit errorOccurred(message.header.value("message").toString(QStringLiteral("未知协议错误")));
        break;
    }
}

void LanNetworkClient::onTcpDisconnected(TcpConnection *connection)
{
    removeConnection(connection);
    connection->deleteLater();
}

void LanNetworkClient::onTcpError(TcpConnection *connection, const QString &message)
{
    Q_UNUSED(connection)
    emit errorOccurred(message);
}

void LanNetworkClient::sendPendingChunks()
{
    bool hasActive = false;
    const QList<QString> ids = m_outgoingTransfers.keys();

    for (const QString &transferId : ids) {
        if (!m_outgoingTransfers.contains(transferId)) {
            continue;
        }

        OutgoingTransfer &transfer = m_outgoingTransfers[transferId];
        if (!transfer.accepted) {
            continue;
        }

        if (transfer.nextChunk >= static_cast<int>(transfer.chunks.size())) {
            continue;
        }

        hasActive = true;
        const FileChunk &chunk = transfer.chunks[transfer.nextChunk];
        const std::vector<uint8_t> wire = chunk.serialize();
        const QByteArray payload(reinterpret_cast<const char *>(wire.data()), static_cast<int>(wire.size()));

        QJsonObject header;
        header["transferId"] = transferId;
        header["chunkId"] = static_cast<int>(chunk.chunk_id);

        if (!sendToPeer(transfer.view.peerId, makeMessage(WireMessageType::FileChunk, header, payload))) {
            transfer.view.state = TransferState::Failed;
            transfer.view.message = QStringLiteral("发送分块失败");
            emitTransfer(transfer.view);
            cleanupOutgoingTempFile(transfer);
            m_outgoingTransfers.remove(transferId);
            continue;
        }

        ++transfer.nextChunk;
        transfer.view.state = TransferState::Transferring;
        transfer.view.progress = transfer.chunks.empty()
            ? 0
            : (transfer.nextChunk * 100) / static_cast<int>(transfer.chunks.size());
        transfer.view.message = QStringLiteral("已发送 %1/%2 块")
            .arg(transfer.nextChunk)
            .arg(static_cast<int>(transfer.chunks.size()));
        emitTransfer(transfer.view);

        if (transfer.nextChunk >= static_cast<int>(transfer.chunks.size())) {
            QJsonObject done;
            done["transferId"] = transferId;
            sendToPeer(transfer.view.peerId, makeMessage(WireMessageType::FileComplete, done));

            transfer.view.progress = 100;
            transfer.view.state = TransferState::Finished;
            transfer.view.message = QStringLiteral("发送完成");
            emitTransfer(transfer.view);
            cleanupOutgoingTempFile(transfer);
            m_outgoingTransfers.remove(transferId);
        }
    }

    if (!hasActive) {
        m_sendTimer.stop();
    }
}

void LanNetworkClient::registerMetaTypes()
{
    qRegisterMetaType<PeerInfo>("PeerInfo");
    qRegisterMetaType<FileOfferInfo>("FileOfferInfo");
    qRegisterMetaType<TransferView>("TransferView");
}

void LanNetworkClient::updatePeer(const PeerInfo &peer)
{
    const bool isNew = !m_peers.contains(peer.peerId);
    m_peers.insert(peer.peerId, peer);
    emit peerOnline(peer);

    if (isNew) {
        broadcastHello();
    }
}

void LanNetworkClient::removeConnection(TcpConnection *connection)
{
    const QString peerId = m_peerByConnection.take(connection);

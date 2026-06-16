    OutgoingTransfer transfer;
    transfer.view = view;
    transfer.chunks = std::move(chunks);
    m_outgoingTransfers.insert(transferId, std::move(transfer));
    emitTransfer(view);

    QJsonObject header;
    header["transferId"] = transferId;
    header["fileName"] = info.fileName();
    header["fileSize"] = QString::number(info.size());
    header["totalChunks"] = static_cast<int>(m_outgoingTransfers[transferId].chunks.size());

    if (!sendToPeer(peerId, makeMessage(WireMessageType::FileStart, header))) {
        TransferView failed = view;
        failed.state = TransferState::Failed;
        failed.message = QStringLiteral("发送文件请求失败");
        m_outgoingTransfers.remove(transferId);
        emitTransfer(failed);
    }
}

void LanNetworkClient::acceptFile(const QString &transferId)
{
    if (!m_pendingIncomingOffers.contains(transferId)) {
        return;
    }

    const FileOfferInfo offer = m_pendingIncomingOffers.take(transferId).offer;
    const QString fileName = uniqueDownloadFileName(offer.fileName);
    const bool ok = m_fileManager.createTransferSession(transferId.toStdString(),
                                                        fileName.toStdString(),
                                                        offer.totalChunks);
    if (!ok) {
        emit errorOccurred(QStringLiteral("创建接收会话失败：%1").arg(fileName));
        return;
    }

    TransferView view;
    view.transferId = transferId;
    view.peerId = offer.fromPeerId;
    view.peerName = offer.fromPeerName;
    view.fileName = fileName;
    view.localPath = QDir(downloadDirectory()).absoluteFilePath(fileName);
    view.fileSize = offer.fileSize;
    view.direction = TransferDirection::Download;
    view.state = TransferState::Transferring;
    view.progress = 0;
    view.message = QStringLiteral("正在接收");
    emitTransfer(view);

    QJsonObject header;
    header["transferId"] = transferId;
    sendToPeer(offer.fromPeerId, makeMessage(WireMessageType::FileAccept, header));
}

void LanNetworkClient::rejectFile(const QString &transferId)
{
    if (!m_pendingIncomingOffers.contains(transferId)) {
        return;
    }

    const FileOfferInfo offer = m_pendingIncomingOffers.take(transferId).offer;

    QJsonObject header;
    header["transferId"] = transferId;
    sendToPeer(offer.fromPeerId, makeMessage(WireMessageType::FileReject, header));
}

void LanNetworkClient::cancelTransfer(const QString &transferId)
{
    if (m_outgoingTransfers.contains(transferId)) {
        OutgoingTransfer transfer = m_outgoingTransfers.take(transferId);
        transfer.view.state = TransferState::Cancelled;
        transfer.view.message = QStringLiteral("已取消发送");
        emitTransfer(transfer.view);

        QJsonObject header;
        header["transferId"] = transferId;
        sendToPeer(transfer.view.peerId, makeMessage(WireMessageType::FileCancel, header));
        return;
    }

    if (m_transferViews.contains(transferId)) {
        TransferView view = m_transferViews.value(transferId);
        m_fileManager.cancelTransfer(transferId.toStdString());
        view.state = TransferState::Cancelled;
        view.message = QStringLiteral("已取消接收");
        emitTransfer(view);

        QJsonObject header;
        header["transferId"] = transferId;
        sendToPeer(view.peerId, makeMessage(WireMessageType::FileCancel, header));
    }
}

void LanNetworkClient::setDownloadDirectory(const QString &dir)
{
    if (dir.isEmpty()) {
        return;
    }

    if (!m_fileManager.setDownloadDirectory(dir.toStdString())) {
        emit errorOccurred(QStringLiteral("下载目录不可用：%1").arg(dir));
    }
}

void LanNetworkClient::addManualPeer(const QString &displayName, const QString &address, quint16 tcpPort)
{
    if (!m_running) {
        emit errorOccurred(QStringLiteral("请先启动局域网客户端"));
        return;
    }

    QHostAddress host;
    if (!host.setAddress(address.trimmed()) || host.protocol() != QAbstractSocket::IPv4Protocol) {
        emit errorOccurred(QStringLiteral("手动连接失败：IP 地址格式不正确"));
        return;
    }

    if (tcpPort == 0) {
        emit errorOccurred(QStringLiteral("手动连接失败：端口不正确"));
        return;
    }

    const QHostAddress normalized = normalizedPeerAddress(host);
    PeerInfo peer;
    peer.peerId = QStringLiteral("manual:%1:%2").arg(normalized.toString()).arg(tcpPort);
    peer.name = displayName.trimmed().isEmpty()
        ? QStringLiteral("手动用户 %1:%2").arg(normalized.toString()).arg(tcpPort)
        : displayName.trimmed();
    peer.address = normalized;
    peer.tcpPort = tcpPort;
    peer.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

    updatePeer(peer);
    emit errorOccurred(QStringLiteral("已添加手动连接：%1:%2").arg(peer.address.toString()).arg(peer.tcpPort));
}

void LanNetworkClient::broadcastHello()
{
    if (!m_running) {
        return;
    }

    QJsonObject hello;
    hello["app"] = "LanChatQt";
    hello["version"] = 1;
    hello["peerId"] = m_localPeerId;
    hello["name"] = m_localName;
    hello["tcpPort"] = static_cast<int>(m_tcpPort);
    hello["timestampMs"] = QString::number(QDateTime::currentMSecsSinceEpoch());

    const QByteArray data = QJsonDocument(hello).toJson(QJsonDocument::Compact);
    m_udpSocket.writeDatagram(data, QHostAddress::Broadcast, DiscoveryPort);
    m_udpSocket.writeDatagram(data, QHostAddress::LocalHost, DiscoveryPort);
}

void LanNetworkClient::readDiscoveryDatagrams()
{
    while (m_udpSocket.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_udpSocket.pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort = 0;
        m_udpSocket.readDatagram(data.data(), data.size(), &sender, &senderPort);
        Q_UNUSED(senderPort)

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            continue;
        }

        const QJsonObject obj = doc.object();
        if (obj.value("app").toString() != "LanChatQt") {
            continue;
        }


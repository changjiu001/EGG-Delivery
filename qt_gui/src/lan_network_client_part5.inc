    }

    if (!m_fileManager.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data)) {
        emit errorOccurred(QStringLiteral("写入文件分块失败"));
        return;
    }

    TransferView view = m_transferViews.value(transferId);
    view.progress = qMax(0, m_fileManager.getSessionProgress(chunk.file_hash));
    view.state = TransferState::Transferring;
    view.message = QStringLiteral("正在接收：%1%").arg(view.progress);
    emitTransfer(view);

    if (m_fileManager.isTransferComplete(chunk.file_hash)) {
        const bool ok = m_fileManager.completeTransfer(chunk.file_hash);
        view.progress = 100;
        view.state = ok ? TransferState::Finished : TransferState::Failed;
        view.message = ok ? QStringLiteral("接收完成") : QStringLiteral("文件组装失败");
        emitTransfer(view);
    }
}

void LanNetworkClient::handleFileComplete(const WireMessage &message)
{
    Q_UNUSED(message)
}

void LanNetworkClient::handleFileCancel(const WireMessage &message)
{
    const QString transferId = message.header.value("transferId").toString();
    if (transferId.isEmpty()) {
        return;
    }

    m_fileManager.cancelTransfer(transferId.toStdString());

    if (m_outgoingTransfers.contains(transferId)) {
        OutgoingTransfer transfer = m_outgoingTransfers.take(transferId);
        transfer.view.state = TransferState::Cancelled;
        transfer.view.message = QStringLiteral("对方取消传输");
        emitTransfer(transfer.view);
        return;
    }

    if (m_pendingIncomingOffers.contains(transferId)) {
        const FileOfferInfo offer = m_pendingIncomingOffers.take(transferId).offer;
        TransferView view;
        view.transferId = transferId;
        view.peerId = offer.fromPeerId;
        view.peerName = offer.fromPeerName;
        view.fileName = offer.fileName;
        view.localPath = QDir(downloadDirectory()).absoluteFilePath(offer.fileName);
        view.fileSize = offer.fileSize;
        view.direction = TransferDirection::Download;
        view.state = TransferState::Cancelled;
        view.progress = 0;
        view.message = QStringLiteral("对方取消传输");
        emitTransfer(view);
        return;
    }

    if (m_transferViews.contains(transferId)) {
        TransferView view = m_transferViews.value(transferId);
        view.state = TransferState::Cancelled;
        view.message = QStringLiteral("对方取消传输");
        emitTransfer(view);
    }
}

QString LanNetworkClient::uniqueDownloadFileName(const QString &fileName) const
{
    const QString safeName = safeFileName(fileName);
    const QDir dir(downloadDirectory());
    const QFileInfo info(safeName);
    const QString base = info.completeBaseName().isEmpty() ? QStringLiteral("file") : info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = safeName;
    int index = 1;
    while (dir.exists(candidate)) {
        candidate = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(index)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(index).arg(suffix);
        ++index;
    }
    return candidate;
}

QString LanNetworkClient::safeFileName(const QString &fileName) const
{
    QString name = QFileInfo(fileName).fileName().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("received_file");
    }

    static const QRegularExpression invalid(R"([\\/:*?"<>|])");
    name.replace(invalid, "_");
    return name;
}

QString LanNetworkClient::peerName(const QString &peerId) const
{
    if (m_peers.contains(peerId)) {
        return shortPeerName(m_peers.value(peerId));
    }
    return peerId.left(8);
}

void LanNetworkClient::emitTransfer(const TransferView &view)
{
    const bool created = !m_transferViews.contains(view.transferId);
    m_transferViews.insert(view.transferId, view);

    if (created) {
        emit transferCreated(view);
    } else {
        emit transferUpdated(view);
    }
}

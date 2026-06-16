    const QString html = QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='2' style='margin:8px 0;'>"
        "<tr><td align='%1'>"
        "<table cellspacing='0' cellpadding='0' style='max-width:72%;'>"
        "<tr><td style='background-color:%2;border:1px solid %3;border-radius:14px;padding:9px 12px;'>"
        "<span style='color:%4;font-size:12px;font-weight:700;'>%5</span> "
        "<span style='color:#cbd5e1;font-size:11px;'>%6</span><br>"
        "<span style='color:#ffffff;font-size:14px;line-height:1.45;'>%7</span>"
        "</td></tr></table>"
        "</td></tr></table>")
        .arg(align,
             bubbleColor,
             borderColor,
             nameColor,
             name.toHtmlEscaped(),
             time.toString("HH:mm:ss"),
             QString(text).toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")));

    m_chatView->append(html);
}

void MainWindow::appendStickerLine(const QString &name, const QString &stickerId, const QColor &color, qint64 timestampMs)
{
    const QDateTime time = timestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(timestampMs)
        : QDateTime::currentDateTime();

    const QString stickerPath = stickerPathById(stickerId);
    if (stickerPath.isEmpty()) {
        appendChatLine(name, QStringLiteral("[表情包]"), color, timestampMs);
        return;
    }

    const bool outgoing = name == QStringLiteral("我");
    const QString align = outgoing ? QStringLiteral("right") : QStringLiteral("left");
    const QString bubbleColor = outgoing ? QStringLiteral("#312e81") : QStringLiteral("#1f2937");
    const QString borderColor = outgoing ? QStringLiteral("#c4b5fd") : QStringLiteral("#475569");

    const QString html = QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='2' style='margin:8px 0;'>"
        "<tr><td align='%1'>"
        "<table cellspacing='0' cellpadding='0'>"
        "<tr><td style='background-color:%2;border:1px solid %3;border-radius:16px;padding:9px 10px;'>"
        "<span style='color:%4;font-size:12px;font-weight:700;'>%5</span> "
        "<span style='color:#cbd5e1;font-size:11px;'>%6</span><br>"
        "<img src='%7' width='132'/>"
        "</td></tr></table>"
        "</td></tr></table>")
        .arg(align,
             bubbleColor,
             borderColor,
             color.name(),
             name.toHtmlEscaped(),
             time.toString("HH:mm:ss"),
             stickerPath.toHtmlEscaped());

    m_chatView->append(html);
}

void MainWindow::sendStickerById(const QString &stickerId)
{
    const QString peerId = selectedPeerId();
    if (peerId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个在线用户。"));
        return;
    }

    if (stickerId.trimmed().isEmpty()) {
        return;
    }

    m_network.sendSticker(peerId, stickerId);
    appendStickerLine(QStringLiteral("我"), stickerId, QColor(96, 165, 250));
}

void MainWindow::setUiRunning(bool running, quint16 port)
{
    m_nameEdit->setEnabled(!running);
    m_portSpin->setEnabled(!running);
    m_startButton->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
    if (m_manualIpEdit) m_manualIpEdit->setEnabled(running);
    if (m_manualPortSpin) m_manualPortSpin->setEnabled(running);
    if (m_manualAddButton) m_manualAddButton->setEnabled(running);
    m_statusLabel->setText(running
        ? QStringLiteral("状态：已启动\n端口：%1").arg(port)
        : QStringLiteral("状态：未启动"));
}

void MainWindow::upsertTransferRow(const TransferView &transfer)
{
    int row = rowForTransfer(transfer.transferId);
    if (row < 0) {
        row = m_transferTable->rowCount();
        m_transferTable->insertRow(row);
    }

    auto setItem = [&](int column, const QString &text) {
        auto *item = m_transferTable->item(row, column);
        if (!item) {
            item = new QTableWidgetItem;
            m_transferTable->setItem(row, column, item);
        }
        item->setText(text);
        item->setData(Qt::UserRole, transfer.transferId);
    };

    setItem(0, transfer.fileName);
    setItem(1, transfer.peerName);
    setItem(2, transfer.direction == TransferDirection::Upload ? QStringLiteral("发送") : QStringLiteral("接收"));
    setItem(3, formatBytes(transfer.fileSize));
    setItem(4, formatTransferTime(transfer.updatedAtMs));
    setItem(5, transfer.localPath.isEmpty() ? QStringLiteral("-") : QDir::toNativeSeparators(transfer.localPath));
    setItem(7, transfer.message.isEmpty() ? stateText(transfer.state) : stateText(transfer.state) + QStringLiteral("：") + transfer.message);

    if (auto *pathItem = m_transferTable->item(row, 5)) {
        pathItem->setToolTip(pathItem->text());
    }

    m_transferTable->setCellWidget(row, 6, makeProgressCell(transfer.progress));
    m_transferTable->setCellWidget(row, 8, makeActionCell(transfer));
}

int MainWindow::rowForTransfer(const QString &transferId) const
{
    for (int row = 0; row < m_transferTable->rowCount(); ++row) {
        const QTableWidgetItem *item = m_transferTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == transferId) {
            return row;
        }
    }
    return -1;
}

QWidget *MainWindow::makeProgressCell(int progress) const
{
    auto *bar = new QProgressBar;
    bar->setRange(0, 100);
    bar->setValue(qBound(0, progress, 100));
    bar->setTextVisible(true);
    return bar;
}

QWidget *MainWindow::makeActionCell(const TransferView &transfer)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    const bool incomingPending = transfer.direction == TransferDirection::Download
        && transfer.state == TransferState::Waiting;
    if (incomingPending) {
        auto *acceptButton = new QPushButton(QStringLiteral("接收"), container);
        auto *rejectButton = new QPushButton(QStringLiteral("拒绝"), container);
        acceptButton->setMinimumWidth(58);
        rejectButton->setMinimumWidth(58);

        connect(acceptButton, &QPushButton::clicked, this, [this, transferId = transfer.transferId]() {
            acceptPendingTransfer(transferId);
        });
        connect(rejectButton, &QPushButton::clicked, this, [this, transferId = transfer.transferId]() {
            rejectPendingTransfer(transferId);
        });

        layout->addWidget(acceptButton);
        layout->addWidget(rejectButton);
        return container;
    }

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), container);
    cancelButton->setMinimumWidth(58);
    const bool canCancel = transfer.state == TransferState::Waiting
        || transfer.state == TransferState::Transferring;
    cancelButton->setEnabled(canCancel);

    connect(cancelButton, &QPushButton::clicked, this, [this, transferId = transfer.transferId]() {
        m_network.cancelTransfer(transferId);
    });

    layout->addWidget(cancelButton);
    return container;
}

void MainWindow::acceptPendingTransfer(const QString &transferId)
{
    if (!m_transfers.contains(transferId)) {
        return;
    }

    TransferView view = m_transfers.value(transferId);
    view.message = QStringLiteral("已接收，等待文件分块");
    view.state = TransferState::Transferring;
    view.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_transfers.insert(transferId, view);
    upsertTransferRow(view);
    saveTransferHistory();
    m_network.acceptFile(transferId);
}

void MainWindow::rejectPendingTransfer(const QString &transferId)
{
    if (!m_transfers.contains(transferId)) {
        return;
    }

    m_network.rejectFile(transferId);

    TransferView view = m_transfers.value(transferId);
    view.state = TransferState::Rejected;
    view.message = QStringLiteral("你已拒绝接收");
    view.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_transfers.insert(transferId, view);
    upsertTransferRow(view);
    saveTransferHistory();
}

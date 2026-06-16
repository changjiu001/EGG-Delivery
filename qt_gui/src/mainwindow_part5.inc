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
    setItem(5, transfer.message.isEmpty() ? stateText(transfer.state) : stateText(transfer.state) + QStringLiteral("：") + transfer.message);

    m_transferTable->setCellWidget(row, 4, makeProgressCell(transfer.progress));
    m_transferTable->setCellWidget(row, 6, makeCancelButton(transfer.transferId, transfer.state));
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

QPushButton *MainWindow::makeCancelButton(const QString &transferId, TransferState state)
{
    auto *button = new QPushButton(QStringLiteral("取消"));
    const bool enabled = state == TransferState::Waiting || state == TransferState::Transferring;
    button->setEnabled(enabled);

    connect(button, &QPushButton::clicked, this, [this, transferId]() {
        m_network.cancelTransfer(transferId);
    });

    return button;
}

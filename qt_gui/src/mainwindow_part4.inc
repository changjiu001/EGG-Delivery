QString MainWindow::selectedPeerId() const
{
    QListWidgetItem *item = m_peerList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QListWidgetItem *MainWindow::findPeerItem(const QString &peerId) const
{
    for (int i = 0; i < m_peerList->count(); ++i) {
        QListWidgetItem *item = m_peerList->item(i);
        if (item->data(Qt::UserRole).toString() == peerId) {
            return item;
        }
    }
    return nullptr;
}

void MainWindow::appendChatLine(const QString &name, const QString &text, const QColor &color, qint64 timestampMs)
{
    const QDateTime time = timestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(timestampMs)
        : QDateTime::currentDateTime();

    const bool outgoing = name == QStringLiteral("我");
    const QString align = outgoing ? QStringLiteral("right") : QStringLiteral("left");
    const QString bubbleColor = outgoing ? QStringLiteral("#1c2d41") : QStringLiteral("#21262d");
    const QString borderColor = outgoing ? QStringLiteral("#303f5c") : QStringLiteral("#30363d");
    const QString nameColor = color.name();


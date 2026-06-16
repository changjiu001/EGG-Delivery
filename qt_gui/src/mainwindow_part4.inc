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
    const QString bubbleColor = outgoing ? QStringLiteral("#2563eb") : QStringLiteral("#1f2937");
    const QString borderColor = outgoing ? QStringLiteral("#93c5fd") : QStringLiteral("#475569");
    const QString nameColor = color.name();


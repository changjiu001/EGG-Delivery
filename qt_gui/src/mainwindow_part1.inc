#include "mainwindow.h"

#include "sticker_catalog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QToolButton>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    connectSignals();
    applyStyle();

    const QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        + QStringLiteral("/LanChatDownloads");
    QDir().mkpath(downloadDir);
    m_network.setDownloadDirectory(downloadDir);
    m_downloadDirLabel->setText(downloadDir);
}

void MainWindow::toggleNetwork()
{
    if (m_network.isRunning()) {
        m_network.stop();
        return;
    }

    const QString dir = m_downloadDirLabel->text();
    const bool ok = m_network.start(m_nameEdit->text(),
                                    static_cast<quint16>(m_portSpin->value()),
                                    dir);
    if (!ok) {
        setUiRunning(false);
    }
}

void MainWindow::sendCurrentText()
{
    const QString peerId = selectedPeerId();
    const QString text = m_messageEdit->text().trimmed();

    if (peerId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个在线用户。"));
        return;
    }

    if (text.isEmpty()) {
        return;
    }

    m_network.sendText(peerId, text);
    appendChatLine(QStringLiteral("我"), text, QColor(96, 165, 250));
    m_messageEdit->clear();
}

void MainWindow::chooseAndSendFile()
{
    const QString peerId = selectedPeerId();
    if (peerId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个在线用户。"));
        return;
    }

    QFileDialog dialog(this, QStringLiteral("选择要发送的文件"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (!dialog.exec()) {
        return;
    }

    const QStringList files = dialog.selectedFiles();
    if (files.isEmpty() || files.first().isEmpty()) {
        return;
    }

    m_network.sendFile(peerId, files.first());
}

void MainWindow::chooseDownloadDirectory()
{
    QFileDialog dialog(this, QStringLiteral("选择接收文件保存目录"), m_downloadDirLabel->text());
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setFileMode(QFileDialog::Directory);
    if (!dialog.exec()) {
        return;
    }

    const QStringList dirs = dialog.selectedFiles();
    if (dirs.isEmpty() || dirs.first().isEmpty()) {
        return;
    }

    const QString dir = dirs.first();
    QDir().mkpath(dir);
    m_network.setDownloadDirectory(dir);
    m_downloadDirLabel->setText(dir);
}

void MainWindow::openDownloadDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadDirLabel->text()));
}

void MainWindow::clearFinishedTransfers()
{
    for (int row = m_transferTable->rowCount() - 1; row >= 0; --row) {
        const QString transferId = m_transferTable->item(row, 0)->data(Qt::UserRole).toString();
        const TransferView view = m_transfers.value(transferId);
        if (view.state == TransferState::Finished
            || view.state == TransferState::Failed
            || view.state == TransferState::Cancelled
            || view.state == TransferState::Rejected) {
            m_transferTable->removeRow(row);
        }
    }
}

void MainWindow::addManualPeer()
{
    if (!m_network.isRunning()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先启动本机客户端。"));
        return;
    }

    const QString ip = m_manualIpEdit->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请输入对方 IPv4 地址。"));
        return;
    }

    m_network.addManualPeer(QString(), ip, static_cast<quint16>(m_manualPortSpin->value()));
}

void MainWindow::onStarted(quint16 port)
{
    setUiRunning(true, port);
    statusBar()->showMessage(QStringLiteral("已启动，正在发现局域网用户"));
}

void MainWindow::onStopped()
{
    setUiRunning(false);
    m_peerList->clear();
    m_peers.clear();
    statusBar()->showMessage(QStringLiteral("已停止"));
}

void MainWindow::onPeerOnline(const PeerInfo &peer)
{
    m_peers.insert(peer.peerId, peer);

    QListWidgetItem *item = findPeerItem(peer.peerId);
    if (!item) {
        item = new QListWidgetItem(m_peerList);
        item->setData(Qt::UserRole, peer.peerId);
    }

    item->setText(QStringLiteral("%1\n%2:%3")
                      .arg(shortPeerName(peer))
                      .arg(peer.address.toString())
                      .arg(peer.tcpPort));
}

void MainWindow::onPeerOffline(const QString &peerId)
{
    m_peers.remove(peerId);
    QListWidgetItem *item = findPeerItem(peerId);
    if (item) {
        delete item;
    }
}

void MainWindow::onTextReceived(const QString &peerId, const QString &peerName, const QString &text, qint64 timestampMs)
{
    Q_UNUSED(peerId)
    appendChatLine(peerName, text, QColor(52, 211, 153), timestampMs);
}

void MainWindow::onStickerReceived(const QString &peerId, const QString &peerName, const QString &stickerId, qint64 timestampMs)
{
    Q_UNUSED(peerId)
    appendStickerLine(peerName, stickerId, QColor(250, 204, 21), timestampMs);
}

void MainWindow::onTextSendFailed(const QString &peerId, const QString &reason)
{
    Q_UNUSED(peerId)
    QMessageBox::warning(this, QStringLiteral("发送失败"), reason);
}

void MainWindow::onFileOfferReceived(const FileOfferInfo &offer)
{
    const QString message = QStringLiteral("%1 想发送文件：\n\n%2\n大小：%3\n\n是否接收？")
        .arg(offer.fromPeerName)
        .arg(offer.fileName)
        .arg(formatBytes(offer.fileSize));


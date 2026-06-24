#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_types.h"
#include "lan_network_client.h"

#include <QColor>
#include <QPoint>
#include <QHash>
#include <QListWidget>
#include <QMainWindow>
#include <QProgressBar>
#include <QStringList>
#include <QTableWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void toggleNetwork();
    void sendCurrentText();
    void chooseAndSendFile();
    void chooseAndSendFolder();
    void chooseDownloadDirectory();
    void openDownloadDirectory();
    void clearFinishedTransfers();
    void deleteSelectedTransfer();
    void showTransferContextMenu(const QPoint &position);
    void addManualPeer();

    void onStarted(quint16 port);
    void onStopped();
    void onPeerOnline(const PeerInfo &peer);
    void onPeerOffline(const QString &peerId);
    void onTextReceived(const QString &peerId, const QString &peerName, const QString &text, qint64 timestampMs);
    void onStickerReceived(const QString &peerId, const QString &peerName, const QString &stickerId, qint64 timestampMs);
    void onTextSendFailed(const QString &peerId, const QString &reason);
    void onFileOfferReceived(const FileOfferInfo &offer);
    void onTransferCreated(const TransferView &transfer);
    void onTransferUpdated(const TransferView &transfer);
    void onNetworkError(const QString &message);

private:
    void buildUi();
    void buildLeftPanel(QWidget *parent);
    void buildChatTab(QWidget *tab);
    void buildTransferTab(QWidget *tab);
    void connectSignals();
    void applyStyle();

    QString selectedPeerId() const;
    QListWidgetItem *findPeerItem(const QString &peerId) const;
    void appendChatLine(const QString &name, const QString &text, const QColor &color, qint64 timestampMs = 0);
    void appendStickerLine(const QString &name, const QString &stickerId, const QColor &color, qint64 timestampMs = 0);
    void sendStickerById(const QString &stickerId);
    void setUiRunning(bool running, quint16 port = 0);
    void upsertTransferRow(const TransferView &transfer);
    int rowForTransfer(const QString &transferId) const;
    QWidget *makeProgressCell(int progress) const;
    QWidget *makeActionCell(const TransferView &transfer);
    void acceptPendingTransfer(const QString &transferId);
    void rejectPendingTransfer(const QString &transferId);
    QStringList selectedTransferIds() const;
    void deleteTransferIds(const QStringList &transferIds);
    void loadTransferHistory();
    void saveTransferHistory() const;
    QString transferHistoryFilePath() const;

    LanNetworkClient m_network;

    QLineEdit *m_nameEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_manualIpEdit = nullptr;
    QSpinBox *m_manualPortSpin = nullptr;
    QPushButton *m_manualAddButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QListWidget *m_peerList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_downloadDirLabel = nullptr;

    QTextBrowser *m_chatView = nullptr;
    QLineEdit *m_messageEdit = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_fileButton = nullptr;
    QPushButton *m_folderButton = nullptr;
    QPushButton *m_stickerToggleButton = nullptr;
    QWidget *m_stickerPanel = nullptr;


    QTableWidget *m_transferTable = nullptr;
    QPushButton *m_dirButton = nullptr;
    QPushButton *m_openDirButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_deleteButton = nullptr;

    QHash<QString, PeerInfo> m_peers;
    QHash<QString, TransferView> m_transfers;
};

#endif // MAINWINDOW_H

#ifndef FILE_TRANSFER_CONTROLLER_H
#define FILE_TRANSFER_CONTROLLER_H

#include "file_manager.h"

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <cstddef>
#include <vector>

struct FileStartEnvelope {
    QString fileHash;
    QString fileName;
    quint32 totalChunks = 0;
    quint64 fileSize = 0;
};

struct TransferRecord {
    QString transferId;
    QString fileHash;
    QString fileName;
    QString direction;
    QString peerName;
    QString status;
    QString detail;
    int progress = 0;
    quint64 bytesDone = 0;
    quint64 bytesTotal = 0;
    quint32 chunksDone = 0;
    quint32 chunksTotal = 0;
    bool cancellable = true;
};

Q_DECLARE_METATYPE(FileStartEnvelope)
Q_DECLARE_METATYPE(TransferRecord)

class FileTransferController : public QObject {
    Q_OBJECT

public:
    explicit FileTransferController(const QString& downloadDirectory, QObject* parent = nullptr);

    QString downloadDirectory() const;
    bool loopbackEnabled() const;

public slots:
    void setDownloadDirectory(const QString& directory);
    void setLoopbackEnabled(bool enabled);
    void sendFile(const QString& filePath, const QString& peerName = QStringLiteral("LAN peer"));
    void cancelTransfer(const QString& transferId);

    void handleIncomingFileStart(const FileStartEnvelope& envelope,
                                 const QString& peerName = QStringLiteral("LAN peer"));
    void handleIncomingFileChunk(const QByteArray& wireBytes);
    void handleIncomingFileCancel(const QString& fileHash);

signals:
    void transferAdded(const TransferRecord& record);
    void transferUpdated(const TransferRecord& record);
    void transferCompleted(const TransferRecord& record);
    void transferFailed(const QString& transferId, const QString& reason);
    void logMessage(const QString& message);

    // Connect these signals to Module 2 / Module 1 when the network stack is ready.
    void outgoingFileStart(const FileStartEnvelope& envelope);
    void outgoingFileChunk(const QByteArray& wireBytes);
    void outgoingFileCancel(const QString& fileHash);

private slots:
    void processOutgoingQueue();
    void refreshReceiveProgress();

private:
    struct OutgoingTransfer {
        TransferRecord record;
        std::vector<FileChunk> chunks;
        std::size_t nextChunk = 0;
        bool canceled = false;
    };

    static QString shortHash(const QString& hash);
    static std::string toFileManagerPath(const QString& path);
    static QByteArray toByteArray(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> toByteVector(const QByteArray& bytes);

    QString makeReceiveFileName(const QString& requestedName) const;
    QString makeTransferId(const QString& prefix, const QString& hash) const;
    void failTransfer(const QString& transferId, const QString& reason);

    FileManager fileManager_;
    QTimer sendTimer_;
    QTimer receivePollTimer_;
    bool loopbackEnabled_ = true;

    QHash<QString, QSharedPointer<OutgoingTransfer>> outgoingTransfers_;
    QHash<QString, TransferRecord> records_;
    QHash<QString, QString> activeReceiveByHash_;
    QHash<QString, QString> receiveFileNameByHash_;
};

#endif // FILE_TRANSFER_CONTROLLER_H

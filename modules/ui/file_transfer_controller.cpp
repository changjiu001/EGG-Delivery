#include "file_transfer_controller.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int kSendTickMs = 25;
constexpr int kReceivePollMs = 200;
}

FileTransferController::FileTransferController(const QString& downloadDirectory, QObject* parent)
    : QObject(parent),
      fileManager_(toFileManagerPath(downloadDirectory.empty()
                                         ? QDir::currentPath() + QStringLiteral("/downloads")
                                         : downloadDirectory)) {
    qRegisterMetaType<FileStartEnvelope>("FileStartEnvelope");
    qRegisterMetaType<TransferRecord>("TransferRecord");

    sendTimer_.setInterval(kSendTickMs);
    connect(&sendTimer_, &QTimer::timeout, this, &FileTransferController::processOutgoingQueue);

    receivePollTimer_.setInterval(kReceivePollMs);
    connect(&receivePollTimer_, &QTimer::timeout, this, &FileTransferController::refreshReceiveProgress);
    receivePollTimer_.start();
}

QString FileTransferController::downloadDirectory() const {
    return QString::fromLocal8Bit(fileManager_.getDownloadDirectory().c_str());
}

bool FileTransferController::loopbackEnabled() const {
    return loopbackEnabled_;
}

void FileTransferController::setDownloadDirectory(const QString& directory) {
    if (directory.trimmed().isEmpty()) {
        emit logMessage(QStringLiteral("下载目录不能为空。"));
        return;
    }

    if (!fileManager_.setDownloadDirectory(toFileManagerPath(directory))) {
        emit logMessage(QStringLiteral("设置下载目录失败：%1").arg(directory));
        return;
    }

    emit logMessage(QStringLiteral("下载目录已设置为：%1").arg(directory));
}

void FileTransferController::setLoopbackEnabled(bool enabled) {
    loopbackEnabled_ = enabled;
    emit logMessage(enabled
                        ? QStringLiteral("已开启本机回环演示。发送的文件会模拟一次接收。")
                        : QStringLiteral("已关闭本机回环演示。请连接真实网络模块发送数据。"));
}

void FileTransferController::sendFile(const QString& filePath, const QString& peerName) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        emit logMessage(QStringLiteral("文件不存在：%1").arg(filePath));
        return;
    }

    emit logMessage(QStringLiteral("正在读取并分块：%1").arg(info.fileName()));
    std::vector<FileChunk> chunks = fileManager_.readAndChunkFile(toFileManagerPath(filePath));
    if (chunks.empty()) {
        emit logMessage(QStringLiteral("无法发送空文件或读取失败：%1").arg(info.fileName()));
        return;
    }

    const QString hash = QString::fromStdString(chunks.front().file_hash);
    const QString transferId = makeTransferId(QStringLiteral("send"), hash);

    auto state = QSharedPointer<OutgoingTransfer>::create();
    state->chunks = std::move(chunks);
    state->record.transferId = transferId;
    state->record.fileHash = hash;
    state->record.fileName = info.fileName();
    state->record.direction = QStringLiteral("发送");
    state->record.peerName = peerName;
    state->record.status = QStringLiteral("准备发送");
    state->record.detail = QStringLiteral("%1 个分块，SHA256 %2")
                               .arg(state->chunks.size())
                               .arg(shortHash(hash));
    state->record.bytesTotal = static_cast<quint64>(info.size());
    state->record.chunksTotal = static_cast<quint32>(state->chunks.size());

    outgoingTransfers_.insert(transferId, state);
    records_.insert(transferId, state->record);
    emit transferAdded(state->record);

    FileStartEnvelope envelope;
    envelope.fileHash = hash;
    envelope.fileName = info.fileName();
    envelope.totalChunks = state->record.chunksTotal;
    envelope.fileSize = state->record.bytesTotal;
    emit outgoingFileStart(envelope);

    if (loopbackEnabled_) {
        handleIncomingFileStart(envelope, QStringLiteral("本机回环"));
    }

    if (!sendTimer_.isActive()) {
        sendTimer_.start();
    }
}

void FileTransferController::cancelTransfer(const QString& transferId) {
    if (!records_.contains(transferId)) {
        return;
    }

    TransferRecord record = records_.value(transferId);
    record.status = QStringLiteral("已取消");
    record.cancellable = false;
    record.detail = QStringLiteral("用户取消了传输。");
    records_[transferId] = record;

    if (transferId.startsWith(QStringLiteral("send:"))) {
        if (outgoingTransfers_.contains(transferId)) {
            outgoingTransfers_.value(transferId)->canceled = true;
            outgoingTransfers_.remove(transferId);
        }
        emit outgoingFileCancel(record.fileHash);
        if (loopbackEnabled_) {
            handleIncomingFileCancel(record.fileHash);
        }
    } else if (transferId.startsWith(QStringLiteral("recv:"))) {
        fileManager_.cancelTransfer(toFileManagerPath(record.fileHash));
        activeReceiveByHash_.remove(record.fileHash);
        receiveFileNameByHash_.remove(record.fileHash);
    }

    emit transferUpdated(record);
    emit logMessage(QStringLiteral("已取消：%1").arg(record.fileName));
}

void FileTransferController::handleIncomingFileStart(const FileStartEnvelope& envelope,
                                                     const QString& peerName) {
    if (envelope.fileHash.isEmpty() || envelope.totalChunks == 0) {
        emit logMessage(QStringLiteral("收到无效的文件开始消息。"));
        return;
    }

    if (activeReceiveByHash_.contains(envelope.fileHash)) {
        emit logMessage(QStringLiteral("接收会话已存在：%1").arg(shortHash(envelope.fileHash)));
        return;
    }

    const QString safeName = makeReceiveFileName(envelope.fileName);
    if (!fileManager_.createTransferSession(toFileManagerPath(envelope.fileHash),
                                            toFileManagerPath(safeName),
                                            envelope.totalChunks)) {
        emit logMessage(QStringLiteral("创建接收会话失败：%1").arg(shortHash(envelope.fileHash)));
        return;
    }

    TransferRecord record;
    record.transferId = makeTransferId(QStringLiteral("recv"), envelope.fileHash);
    record.fileHash = envelope.fileHash;
    record.fileName = safeName;
    record.direction = QStringLiteral("接收");
    record.peerName = peerName;
    record.status = QStringLiteral("等待分块");
    record.detail = QStringLiteral("保存到 %1").arg(QDir(downloadDirectory()).filePath(safeName));
    record.bytesTotal = envelope.fileSize;
    record.chunksTotal = envelope.totalChunks;

    activeReceiveByHash_.insert(envelope.fileHash, record.transferId);
    receiveFileNameByHash_.insert(envelope.fileHash, safeName);
    records_.insert(record.transferId, record);

    emit transferAdded(record);
    emit logMessage(QStringLiteral("开始接收：%1").arg(safeName));
}

void FileTransferController::handleIncomingFileChunk(const QByteArray& wireBytes) {
    FileChunk chunk;
    if (!chunk.deserialize(toByteVector(wireBytes))) {
        emit logMessage(QStringLiteral("收到损坏或不支持的文件分块，已丢弃。"));
        return;
    }

    const QString hash = QString::fromStdString(chunk.file_hash);
    if (!activeReceiveByHash_.contains(hash)) {
        FileStartEnvelope envelope;
        envelope.fileHash = hash;
        envelope.fileName = QStringLiteral("received_%1.bin").arg(shortHash(hash));
        envelope.totalChunks = chunk.total_chunks;
        envelope.fileSize = 0;
        handleIncomingFileStart(envelope, QStringLiteral("未知来源"));
    }

    const QString transferId = activeReceiveByHash_.value(hash);
    if (!fileManager_.addChunkToSession(toFileManagerPath(hash), chunk.chunk_id, chunk.data)) {
        failTransfer(transferId, QStringLiteral("写入分块失败：chunk %1").arg(chunk.chunk_id));
        return;
    }

    TransferRecord record = records_.value(transferId);
    const int progress = fileManager_.getSessionProgress(toFileManagerPath(hash));
    record.progress = std::max(0, progress);
    record.status = QStringLiteral("接收中");
    record.chunksDone = static_cast<quint32>((record.chunksTotal * record.progress) / 100);
    record.bytesDone = record.bytesTotal == 0
                           ? 0
                           : static_cast<quint64>((record.bytesTotal * record.progress) / 100);
    record.detail = QStringLiteral("%1 / %2 个分块")
                        .arg(record.chunksDone)
                        .arg(record.chunksTotal);
    records_[transferId] = record;
    emit transferUpdated(record);

    if (fileManager_.isTransferComplete(toFileManagerPath(hash))) {
        const bool ok = fileManager_.completeTransfer(toFileManagerPath(hash));
        record = records_.value(transferId);
        record.progress = ok ? 100 : record.progress;
        record.chunksDone = record.chunksTotal;
        record.bytesDone = record.bytesTotal;
        record.status = ok ? QStringLiteral("已接收") : QStringLiteral("组装失败");
        record.cancellable = false;
        record.detail = ok
                            ? QStringLiteral("已保存到 %1").arg(QDir(downloadDirectory()).filePath(record.fileName))
                            : QStringLiteral("文件组装失败，请检查日志。");
        records_[transferId] = record;
        activeReceiveByHash_.remove(hash);
        receiveFileNameByHash_.remove(hash);

        emit transferUpdated(record);
        if (ok) {
            emit transferCompleted(record);
        } else {
            emit transferFailed(transferId, record.detail);
        }
    }
}

void FileTransferController::handleIncomingFileCancel(const QString& fileHash) {
    const QString transferId = activeReceiveByHash_.value(fileHash);
    if (transferId.isEmpty()) {
        return;
    }

    fileManager_.cancelTransfer(toFileManagerPath(fileHash));
    activeReceiveByHash_.remove(fileHash);
    receiveFileNameByHash_.remove(fileHash);

    TransferRecord record = records_.value(transferId);
    record.status = QStringLiteral("已取消");
    record.cancellable = false;
    record.detail = QStringLiteral("发送方取消了传输。");
    records_[transferId] = record;
    emit transferUpdated(record);
}

void FileTransferController::processOutgoingQueue() {
    QList<QString> finishedIds;

    for (auto it = outgoingTransfers_.begin(); it != outgoingTransfers_.end(); ++it) {
        const QSharedPointer<OutgoingTransfer> state = it.value();
        if (!state || state->canceled) {
            finishedIds.append(it.key());
            continue;
        }

        if (state->nextChunk >= state->chunks.size()) {
            TransferRecord record = state->record;
            record.status = QStringLiteral("已发送");
            record.progress = 100;
            record.chunksDone = record.chunksTotal;
            record.bytesDone = record.bytesTotal;
            record.cancellable = false;
            record.detail = QStringLiteral("所有分块已交给网络模块。");
            records_[record.transferId] = record;
            emit transferUpdated(record);
            emit transferCompleted(record);
            finishedIds.append(it.key());
            continue;
        }

        const FileChunk& chunk = state->chunks.at(state->nextChunk);
        const QByteArray wireBytes = toByteArray(chunk.serialize());
        emit outgoingFileChunk(wireBytes);
        if (loopbackEnabled_) {
            handleIncomingFileChunk(wireBytes);
        }

        state->nextChunk += 1;
        TransferRecord record = state->record;
        record.status = QStringLiteral("发送中");
        record.chunksDone = static_cast<quint32>(state->nextChunk);
        record.progress = static_cast<int>((record.chunksDone * 100) / record.chunksTotal);
        record.bytesDone += static_cast<quint64>(chunk.data.size());
        record.detail = QStringLiteral("%1 / %2 个分块")
                            .arg(record.chunksDone)
                            .arg(record.chunksTotal);
        state->record = record;
        records_[record.transferId] = record;
        emit transferUpdated(record);
    }

    for (const QString& id : finishedIds) {
        outgoingTransfers_.remove(id);
    }

    if (outgoingTransfers_.isEmpty()) {
        sendTimer_.stop();
    }
}

void FileTransferController::refreshReceiveProgress() {
    const std::vector<std::string> activeSessions = fileManager_.getActiveSessions();
    for (const std::string& hashValue : activeSessions) {
        const QString hash = QString::fromStdString(hashValue);
        const QString transferId = activeReceiveByHash_.value(hash);
        if (transferId.isEmpty() || !records_.contains(transferId)) {
            continue;
        }

        TransferRecord record = records_.value(transferId);
        const int progress = fileManager_.getSessionProgress(hashValue);
        if (progress >= 0 && progress != record.progress) {
            record.progress = progress;
            record.status = QStringLiteral("接收中");
            records_[transferId] = record;
            emit transferUpdated(record);
        }
    }
}

QString FileTransferController::shortHash(const QString& hash) {
    return hash.left(12);
}

std::string FileTransferController::toFileManagerPath(const QString& path) {
    return QFile::encodeName(QDir::toNativeSeparators(path)).toStdString();
}

QByteArray FileTransferController::toByteArray(const std::vector<uint8_t>& bytes) {
    return QByteArray(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<int>(bytes.size()));
}

std::vector<uint8_t> FileTransferController::toByteVector(const QByteArray& bytes) {
    const auto* first = reinterpret_cast<const uint8_t*>(bytes.constData());
    return std::vector<uint8_t>(first, first + bytes.size());
}

QString FileTransferController::makeReceiveFileName(const QString& requestedName) const {
    QString baseName = QFileInfo(requestedName).fileName();
    if (baseName.trimmed().isEmpty()) {
        baseName = QStringLiteral("received_file.bin");
    }

    const QDir dir(downloadDirectory());
    QString candidate = baseName;
    const QFileInfo originalInfo(baseName);
    const QString stem = originalInfo.completeBaseName().isEmpty()
                             ? QStringLiteral("received_file")
                             : originalInfo.completeBaseName();
    const QString suffix = originalInfo.suffix();

    int index = 1;
    while (QFileInfo::exists(dir.filePath(candidate))) {
        candidate = suffix.isEmpty()
                        ? QStringLiteral("%1_%2").arg(stem).arg(index)
                        : QStringLiteral("%1_%2.%3").arg(stem).arg(index).arg(suffix);
        ++index;
    }
    return candidate;
}

QString FileTransferController::makeTransferId(const QString& prefix, const QString& hash) const {
    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QStringLiteral("%1:%2:%3").arg(prefix, shortHash(hash), nonce);
}

void FileTransferController::failTransfer(const QString& transferId, const QString& reason) {
    if (!records_.contains(transferId)) {
        emit logMessage(reason);
        return;
    }

    TransferRecord record = records_.value(transferId);
    record.status = QStringLiteral("失败");
    record.detail = reason;
    record.cancellable = false;
    records_[transferId] = record;
    emit transferUpdated(record);
    emit transferFailed(transferId, reason);
}

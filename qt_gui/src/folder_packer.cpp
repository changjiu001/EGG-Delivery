#include "folder_packer.h"

#include "file_manager.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {

struct ZipEntry {
    QString name;
    QByteArray data;
    quint32 crc = 0;
    quint32 localHeaderOffset = 0;
    quint16 modTime = 0;
    quint16 modDate = 0;
};

void toDosDateTime(const QDateTime &dateTime, quint16 &dosTime, quint16 &dosDate)
{
    const QDateTime local = dateTime.toLocalTime();
    const QDate date = local.date();
    const QTime time = local.time();

    dosTime = static_cast<quint16>((time.hour() << 11) | (time.minute() << 5) | (time.second() / 2));
    dosDate = static_cast<quint16>(((date.year() - 1980) << 9) | (date.month() << 5) | date.day());
}

QByteArray encodeName(const QString &name)
{
    return name.toUtf8();
}

void writeUInt16(QIODevice &device, quint16 value)
{
    char bytes[2];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    device.write(bytes, 2);
}

void writeUInt32(QIODevice &device, quint32 value)
{
    char bytes[4];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[3] = static_cast<char>((value >> 24) & 0xFF);
    device.write(bytes, 4);
}

bool writeLocalEntry(QIODevice &device, ZipEntry &entry)
{
    const QByteArray nameBytes = encodeName(entry.name);
    if (nameBytes.isEmpty()) {
        return false;
    }

    entry.localHeaderOffset = static_cast<quint32>(device.pos());

    writeUInt32(device, 0x04034B50);
    writeUInt16(device, 20);
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt16(device, entry.modTime);
    writeUInt16(device, entry.modDate);
    writeUInt32(device, entry.crc);
    writeUInt32(device, static_cast<quint32>(entry.data.size()));
    writeUInt32(device, static_cast<quint32>(entry.data.size()));
    writeUInt16(device, static_cast<quint16>(nameBytes.size()));
    writeUInt16(device, 0);
    device.write(nameBytes);
    if (!entry.data.isEmpty()) {
        if (device.write(entry.data) != entry.data.size()) {
            return false;
        }
    }
    return true;
}

bool writeCentralEntry(QIODevice &device, const ZipEntry &entry)
{
    const QByteArray nameBytes = encodeName(entry.name);

    writeUInt32(device, 0x02014B50);
    writeUInt16(device, 20);
    writeUInt16(device, 20);
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt16(device, entry.modTime);
    writeUInt16(device, entry.modDate);
    writeUInt32(device, entry.crc);
    writeUInt32(device, static_cast<quint32>(entry.data.size()));
    writeUInt32(device, static_cast<quint32>(entry.data.size()));
    writeUInt16(device, static_cast<quint16>(nameBytes.size()));
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt32(device, 0);
    writeUInt32(device, entry.localHeaderOffset);
    device.write(nameBytes);
    return true;
}

bool writeEndRecord(QIODevice &device, quint16 entryCount, quint32 centralSize, quint32 centralOffset)
{
    writeUInt32(device, 0x06054B50);
    writeUInt16(device, 0);
    writeUInt16(device, 0);
    writeUInt16(device, entryCount);
    writeUInt16(device, entryCount);
    writeUInt32(device, centralSize);
    writeUInt32(device, centralOffset);
    writeUInt16(device, 0);
    return true;
}

} // namespace

qint64 FolderPacker::folderByteSize(const QString &folderPath)
{
    qint64 total = 0;
    QDirIterator it(folderPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString FolderPacker::packToZip(const QString &folderPath, const QString &outputDir, QString *errorMessage)
{
    const QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("文件夹不存在：%1").arg(folderPath);
        }
        return {};
    }

    QList<ZipEntry> entries;
    const QDir rootDir(folderInfo.absoluteFilePath());
    QDirIterator it(folderPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取文件：%1").arg(filePath);
            }
            return {};
        }

        ZipEntry entry;
        entry.name = rootDir.relativeFilePath(filePath).replace(QLatin1Char('\\'), QLatin1Char('/'));
        entry.data = file.readAll();
        entry.crc = crc32(entry.data.data(), static_cast<size_t>(entry.data.size()));
        toDosDateTime(it.fileInfo().lastModified(), entry.modTime, entry.modDate);
        entries.append(entry);
    }

    if (entries.isEmpty()) {
        ZipEntry dirEntry;
        dirEntry.name = folderInfo.fileName() + QLatin1Char('/');
        dirEntry.data.clear();
        dirEntry.crc = 0;
        toDosDateTime(folderInfo.lastModified(), dirEntry.modTime, dirEntry.modDate);
        entries.append(dirEntry);
    }

    QDir().mkpath(outputDir);
    const QString zipPath = QDir(outputDir).absoluteFilePath(folderInfo.fileName() + QStringLiteral(".zip"));

    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建压缩包：%1").arg(zipPath);
        }
        return {};
    }

    for (ZipEntry &entry : entries) {
        if (!writeLocalEntry(zipFile, entry)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("写入压缩包失败：%1").arg(entry.name);
            }
            zipFile.remove();
            return {};
        }
    }

    const quint32 centralOffset = static_cast<quint32>(zipFile.pos());
    for (const ZipEntry &entry : entries) {
        if (!writeCentralEntry(zipFile, entry)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("写入压缩目录失败：%1").arg(entry.name);
            }
            zipFile.remove();
            return {};
        }
    }

    const quint32 centralSize = static_cast<quint32>(zipFile.pos() - centralOffset);
    if (!writeEndRecord(zipFile, static_cast<quint16>(entries.size()), centralSize, centralOffset)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("完成压缩包失败");
        }
        zipFile.remove();
        return {};
    }

    zipFile.close();
    return zipPath;
}

#ifndef FOLDER_PACKER_H
#define FOLDER_PACKER_H

#include <QString>

class FolderPacker {
public:
    /** Pack a directory into a zip file (store-only, no compression). */
    static QString packToZip(const QString &folderPath,
                             const QString &outputDir,
                             QString *errorMessage = nullptr);

    static qint64 folderByteSize(const QString &folderPath);
};

#endif // FOLDER_PACKER_H

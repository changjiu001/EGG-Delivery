# EGG-Delivery Qt GUI Integration

This document explains how Module 4 connects to the existing file manager module.

## Build

Install Qt 5 or Qt 6 with the Widgets module, then build from the repository root:

```bash
mkdir build
cd build
cmake .. -DBUILD_GUI=ON
cmake --build .
```

The GUI target is:

```text
egg_delivery_gui
```

If the machine does not have Qt installed and you only want to build Module 3 tests:

```bash
cmake .. -DBUILD_GUI=OFF
```

## Module Boundary

The GUI does not directly manage sockets. It talks to `FileTransferController`.

`FileTransferController` wraps Module 3's `FileManager` and exposes Qt-friendly signals and slots:

| GUI / Network Need | Controller API |
|---|---|
| User chooses a file to send | `sendFile(filePath, peerName)` |
| File start should go to the network | `outgoingFileStart(FileStartEnvelope)` |
| File chunk should go to the network | `outgoingFileChunk(QByteArray)` |
| Network receives a file start message | `handleIncomingFileStart(FileStartEnvelope, peerName)` |
| Network receives a serialized file chunk | `handleIncomingFileChunk(QByteArray)` |
| User cancels a transfer | `cancelTransfer(transferId)` |
| Network receives cancel message | `handleIncomingFileCancel(fileHash)` |

## Protocol Expectations

Module 2 should wrap file transfer messages with a message type:

```cpp
enum class MessageType : uint8_t {
    ChatText = 0x01,
    FileStart = 0x10,
    FileChunk = 0x11,
    FileCancel = 0x13
};
```

For `FileStart`, send these fields:

```cpp
struct FileStartEnvelope {
    QString fileHash;
    QString fileName;
    quint32 totalChunks;
    quint64 fileSize;
};
```

For `FileChunk`, the payload should be exactly the bytes from:

```cpp
FileChunk::serialize()
```

The receiver should pass that payload unchanged to:

```cpp
FileTransferController::handleIncomingFileChunk(QByteArray wireBytes)
```

`FileChunk::deserialize()` already checks protocol magic, version, and CRC32.

## Temporary Loopback Demo

The GUI starts with loopback enabled. This is intentional while Module 1 and Module 2 are incomplete.

In loopback mode:

1. `sendFile()` reads and chunks the file.
2. `outgoingFileStart()` is emitted.
3. The same start message is fed into `handleIncomingFileStart()`.
4. Each serialized chunk is emitted through `outgoingFileChunk()`.
5. The same bytes are fed into `handleIncomingFileChunk()`.
6. The receive side assembles the file and shows progress.

After the real network module is connected, turn loopback off in the UI.

## Suggested Repository Placement

Copy these files into the repository:

```text
CMakeLists.txt
docs/GUI_INTEGRATION.md
modules/ui/main.cpp
modules/ui/main_window.h
modules/ui/main_window.cpp
modules/ui/file_transfer_controller.h
modules/ui/file_transfer_controller.cpp
modules/ui/README.md
```

## Notes

- The GUI passes only base file names to `FileManager` when receiving files, so remote paths cannot escape the download directory.
- Duplicate received file names are renamed automatically.
- `QFile::encodeName()` is used before passing paths into the existing `FileManager` API.
- The controller is the only file that should need changes when Module 1 and Module 2 become available.

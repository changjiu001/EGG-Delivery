# Module 4 - Qt GUI

This folder contains the Qt Widgets GUI for EGG-Delivery.

## Files

| File | Purpose |
|---|---|
| `main.cpp` | Qt application entry point |
| `main_window.h/.cpp` | Main window, transfer table, file picker, download directory controls |
| `file_transfer_controller.h/.cpp` | Qt adapter around Module 3 `FileManager`; exposes signals/slots for Module 1 and Module 2 |

## Current Behavior

- Users can choose a download directory.
- Users can select one or more files to send.
- The GUI calls `FileManager::readAndChunkFile()` to split files.
- The GUI emits `outgoingFileStart()` and `outgoingFileChunk()` for the future network/protocol modules.
- Before Module 1 and Module 2 are ready, "loopback demo" feeds the outgoing messages back into the receive slots so the full UI flow can be tested locally.
- Incoming file progress is shown in the transfer table.
- Completed files are assembled through `FileManager::completeTransfer()`.
- Transfers can be canceled.

## Network Integration Points

When Module 1 and Module 2 are ready, connect the controller like this:

```cpp
connect(controller, &FileTransferController::outgoingFileStart,
        protocol, &ProtocolService::sendFileStart);

connect(controller, &FileTransferController::outgoingFileChunk,
        protocol, &ProtocolService::sendFileChunk);

connect(protocol, &ProtocolService::fileStartReceived,
        controller, &FileTransferController::handleIncomingFileStart);

connect(protocol, &ProtocolService::fileChunkReceived,
        controller, &FileTransferController::handleIncomingFileChunk);

connect(protocol, &ProtocolService::fileCancelReceived,
        controller, &FileTransferController::handleIncomingFileCancel);
```

Then turn off loopback mode in the UI.

# Module 4 - Qt GUI

这个目录存放 EGG-Delivery 的 Qt Widgets 图形界面代码。

## 文件说明

| 文件 | 作用 |
|---|---|
| `main.cpp` | Qt 程序入口 |
| `main_window.h/.cpp` | 主窗口、传输列表、文件选择、下载目录设置、日志显示 |
| `file_transfer_controller.h/.cpp` | Module 3 `FileManager` 的 Qt 适配层，负责向 GUI 暴露 signals/slots，也负责给网络/协议模块预留接口 |

## 当前功能

- 用户可以选择下载目录。
- 用户可以选择一个或多个文件发送。
- GUI 会调用 `FileManager::readAndChunkFile()` 对文件进行切块。
- GUI 会发出 `outgoingFileStart()` 和 `outgoingFileChunk()`，后续可以直接接到网络/协议模块。
- 在 Module 1 和 Module 2 完成前，默认开启“本机回环演示”，把发送出去的消息再模拟喂回接收端。
- 传输列表会显示文件名、发送/接收方向、对端名称、文件哈希、进度和状态。
- 接收完成后，会调用 `FileManager::completeTransfer()` 组装文件。
- 用户可以取消正在进行的传输。

## 和文件管理模块的关系

GUI 不重新实现文件切块、校验和组装逻辑。

这些事情继续由 Module 3 完成：

- `FileManager::readAndChunkFile()`：读取文件并切块
- `FileChunk::serialize()`：把文件块序列化成网络字节
- `FileChunk::deserialize()`：从网络字节恢复文件块，并自动做 CRC32 校验
- `FileManager::createTransferSession()`：创建接收会话
- `FileManager::addChunkToSession()`：保存收到的分块
- `FileManager::getSessionProgress()`：查询接收进度
- `FileManager::completeTransfer()`：分块收齐后写入磁盘
- `FileManager::cancelTransfer()`：取消接收会话

## 网络模块对接点

等 Module 1（网络通信）和 Module 2（数据与协议）完成后，可以把控制器按下面的方式接起来：

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

这里的 `ProtocolService` 只是示例名称，可以替换成实际的数据协议模块类。

## 本机回环演示

当前 GUI 默认开启“本机回环演示”。

这个模式用于在网络模块还没写好时测试 UI：

1. 选择文件发送。
2. 文件被切成多个 `FileChunk`。
3. GUI 显示发送进度。
4. 同一批分块被模拟成“接收数据”。
5. GUI 显示接收进度。
6. 收齐后自动组装到下载目录。

真实网络接好后，在界面里关闭“本机回环演示”即可。

## 构建

在仓库根目录执行：

```bash
mkdir build
cd build
cmake .. -DBUILD_GUI=ON
cmake --build .
```

GUI 目标名：

```text
egg_delivery_gui
```

如果当前环境没有 Qt，可以关闭 GUI 构建：

```bash
cmake .. -DBUILD_GUI=OFF
```

# EGG-Delivery Qt GUI 集成说明

本文档说明 Module 4（UI 与交互）如何接入现有的文件管理模块，以及后续如何和网络通信模块、数据协议模块对接。

## 构建方式

本 GUI 使用 Qt Widgets。请先安装 Qt 5 或 Qt 6，并确保安装了 Widgets 组件。

在仓库根目录执行：

```bash
mkdir build
cd build
cmake .. -DBUILD_GUI=ON
cmake --build .
```

GUI 可执行目标名为：

```text
egg_delivery_gui
```

如果当前机器没有安装 Qt，只想构建文件管理模块、示例程序和测试，可以关闭 GUI：

```bash
cmake .. -DBUILD_GUI=OFF
```

## 模块边界

GUI 不直接管理 Socket，也不直接解析网络外层协议。

当前 UI 通过 `FileTransferController` 访问文件管理模块。这个控制器把 Module 3 的 `FileManager` 包装成 Qt 更容易使用的 signals/slots 接口。

| 需求 | 控制器接口 |
|---|---|
| 用户选择文件并发送 | `sendFile(filePath, peerName)` |
| 文件开始消息交给网络模块发送 | `outgoingFileStart(FileStartEnvelope)` |
| 文件分块数据交给网络模块发送 | `outgoingFileChunk(QByteArray)` |
| 网络模块收到文件开始消息 | `handleIncomingFileStart(FileStartEnvelope, peerName)` |
| 网络模块收到序列化后的文件分块 | `handleIncomingFileChunk(QByteArray)` |
| 用户取消传输 | `cancelTransfer(transferId)` |
| 网络模块收到取消传输消息 | `handleIncomingFileCancel(fileHash)` |

这样做的好处是：GUI 只关心用户操作、传输列表、进度条和日志；网络模块只关心把字节发出去、收回来；文件管理模块继续负责切块、校验、缓存和组装。

## 协议模块需要提供的内容

Module 2（数据与协议）建议给文件传输消息加一层消息类型，例如：

```cpp
enum class MessageType : uint8_t {
    ChatText = 0x01,
    FileStart = 0x10,
    FileChunk = 0x11,
    FileCancel = 0x13
};
```

### 文件开始消息

发送文件前，GUI 会通过 `outgoingFileStart()` 发出这些信息：

```cpp
struct FileStartEnvelope {
    QString fileHash;      // 文件 SHA256
    QString fileName;      // 原始文件名
    quint32 totalChunks;   // 总分块数
    quint64 fileSize;      // 文件总大小
};
```

协议模块需要把这些字段编码进 `FileStart` 消息。接收方解析后，把同样的信息传给：

```cpp
FileTransferController::handleIncomingFileStart(envelope, peerName);
```

### 文件分块消息

文件分块的 payload 应该直接使用：

```cpp
FileChunk::serialize()
```

也就是说，Module 2 不需要重新定义文件分块内部格式，只需要把这段二进制数据作为 `FileChunk` 消息的载荷。

接收方拿到 payload 后，原样传给：

```cpp
FileTransferController::handleIncomingFileChunk(wireBytes);
```

`FileChunk::deserialize()` 已经会自动检查：

- 协议魔数
- 协议版本
- CRC32 校验
- 文件 SHA256 字段

## 网络模块接线示例

等 Module 1 和 Module 2 完成后，可以按下面的方式连接：

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

上面的 `ProtocolService` 是示例名称，实际项目里可以换成你们协议模块自己的类名。

## 本机回环演示

因为网络通信模块和数据协议模块还没有完成，GUI 默认开启“本机回环演示”。

开启后，流程如下：

1. 用户点击“发送文件”。
2. `sendFile()` 调用 `FileManager::readAndChunkFile()` 读取并切块。
3. GUI 发出 `outgoingFileStart()`。
4. 控制器把同一条开始消息喂回 `handleIncomingFileStart()`，模拟接收方创建会话。
5. 每个分块通过 `outgoingFileChunk()` 发出。
6. 控制器把同一段分块字节喂回 `handleIncomingFileChunk()`。
7. 接收侧显示进度。
8. 分块收齐后，调用 `FileManager::completeTransfer()` 组装文件。

这样即使网络模块暂时不可用，也能先测试完整的 GUI 操作流程。

真实网络模块接好后，在界面里关闭“本机回环演示”即可。

## 当前已实现的 GUI 功能

- 选择下载目录
- 打开下载目录
- 选择一个或多个文件发送
- 调用文件模块进行切块
- 显示发送进度
- 显示接收进度
- 显示文件名、方向、对端、哈希、状态
- 取消传输
- 传输日志
- 网络模块未完成时的本机回环演示

## 代码位置

```text
modules/ui/main.cpp
modules/ui/main_window.h
modules/ui/main_window.cpp
modules/ui/file_transfer_controller.h
modules/ui/file_transfer_controller.cpp
modules/ui/README.md
```

## 注意事项

- 接收文件时，GUI 只把文件名传给 `FileManager`，不会使用远端传来的完整路径，避免文件被保存到下载目录之外。
- 如果下载目录里已经存在同名文件，GUI 会自动给新文件名追加序号。
- 传给 `FileManager` 的路径会先经过 `QFile::encodeName()` 转换，方便兼容现有的 `std::string` 接口。
- 后续接入网络模块时，优先修改 `FileTransferController`，尽量不要让 `MainWindow` 直接依赖 Socket 或协议细节。

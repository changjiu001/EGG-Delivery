# EGG-Delivery

无中心服务器的局域网聊天与文件传输系统，使用 C++17、Socket 和 Qt Widgets 实现。

## 模块状态

| 模块 | 负责人 | 状态 |
|---|---|---|
| 网络通信（Socket） | b1t42520 | 已完成 |
| 数据与协议 | 3307687819 | 已完成 |
| 文件与存储管理 | refrain321 | 已完成 |
| UI 与交互（Qt Widgets） | changjiu001 | 已完成 |

## 仓库结构

```text
├─ network_module/          # 原生 Socket 网络模块与命令行测试
├─ protocol/                # 原协议模块
├─ file_manager.cpp/.h      # 文件分块、校验、会话和组装
├─ example.cpp              # FileManager 示例
├─ test.cpp                 # FileManager 测试
├─ qt_gui/                  # 完整 Qt Widgets 客户端
│  ├─ CMakeLists.txt
│  ├─ resources.qrc
│  ├─ assets/
│  └─ src/
├─ API_GUIDE.md
├─ INTEGRATION.md
└─ CMakeLists.txt
```

## Qt 图形界面

图形界面提供：

- UDP 局域网用户发现和手动 IP 连接。
- TCP 点对点文字聊天。
- TCP 文件传输、接收确认、进度显示和取消。
- 聊天气泡和表情包网格面板。
- 同一台电脑双开测试。
- 使用根目录 `FileManager` 进行 64 KB 分块、CRC32 校验、SHA256 标识和文件组装。

Qt Creator 直接打开：

```text
qt_gui/CMakeLists.txt
```

选择 Qt 5/Qt 6 的 Widgets + Network Kit 后构建运行。详细步骤见
[`qt_gui/README.md`](qt_gui/README.md)。

## 根目录 CMake 构建

默认构建原有 FileManager 示例和测试，不强制要求安装 Qt：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

需要从根目录同时构建 Qt GUI 时：

```bash
cmake -S . -B build -DBUILD_QT_GUI=ON
cmake --build build
```

## FileManager 基本接口

```cpp
FileManager fm("./downloads");
auto chunks = fm.readAndChunkFile("file.bin");
for (const auto &chunk : chunks) {
    const auto wire = chunk.serialize();
    // 将 wire 交给网络层发送
}
```

接收端对每个分块反序列化并添加到会话，全部到齐后调用
`completeTransfer()` 完成文件组装。详见 [`API_GUIDE.md`](API_GUIDE.md) 和
[`INTEGRATION.md`](INTEGRATION.md)。

## 网络环境说明

校园 Wi-Fi 可能启用客户端隔离，导致处在同一 SSID 的两台电脑仍不能互相访问。
课程演示优先使用同一个手机热点、普通路由器或允许终端互访的局域网。

# Qt GUI

`qt_gui` 是 EGG-Delivery 的 Qt Widgets 图形界面模块，直接复用仓库根目录的
`file_manager.cpp/.h`，不会覆盖原网络、协议、文件管理和测试代码。

## 功能

- UDP 局域网用户发现，同时支持手动填写 IPv4 和 TCP 端口。
- TCP 点对点文字聊天。
- TCP 点对点文件传输、进度显示、接收确认和取消。
- 使用根目录 `FileManager` 完成文件分块、CRC32、SHA256 和最终组装。
- 聊天气泡界面和表情包网格面板。
- 支持同机双开测试。
- Windows GUI 子系统构建，Release 运行时不弹出控制台窗口。

## Qt Creator 构建

1. 使用 Qt Creator 打开本目录的 `CMakeLists.txt`。
2. 选择包含 Qt Widgets 和 Qt Network 的 Qt 5/Qt 6 Kit。
3. 配置、构建并运行 `LanChatQt`。

推荐环境：Qt 6.11.1 MinGW 64-bit、CMake 3.16 或更高版本。

## 测试

同机双开时使用两个不同端口，例如 `50001` 和 `50002`。

两台电脑测试时应连接允许客户端互访的同一热点或普通路由器。部分校园网启用了
客户端隔离，即使处在同一 Wi-Fi 也可能无法直接通信。自动发现失败时，可在左侧
手动填写对方的 IPv4 地址和程序监听端口。

## Windows 发布

在 Release 构建目录找到 `LanChatQt.exe`，复制到单独目录后执行：

```bat
"D:\QT\6.11.1\mingw_64\bin\windeployqt.exe" --release --compiler-runtime --no-translations LanChatQt.exe
```

发布时需要分发整个目录，而不是只发送 exe。

## 图标说明

窗口和任务栏图标通过 Qt 资源加载。没有启用 Windows `.rc` 文件，避免部分 MinGW
环境出现 `windres.exe preprocessing failed`。资源管理器中的 exe 文件图标因此可能
仍显示为默认图标，但程序构建和运行更稳定。

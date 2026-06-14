# Module 1 网络Socket模块 EGG-Delivery
- 负责人：吕志锋
- 功能：局域网无中心P2P TCP通信，对接FileManager实现文件分块传输
- 固定端口：9090
- Cpp标准：C++17
- 支持编译环境：MinGW、Visual Studio

## 一、项目文件清单
1. simple_socket.h // 跨平台套接字函数声明、类型别名、系统适配宏;
2. simple_socket.cpp //socket 底层接口完整实现;
3. transfer_logic.cpp // 网络层与 FileManager 业务对接逻辑;
4. net_test.cpp // 程序交互启动入口;
5. NETWORK_MODULE.md // 模块说明文档。

---

## 二、模块职责范围
1. 实现Windows/Linux跨平台TCP套接字封装；
2. 建立两台局域网设备点对点无中心连接；
3. 负责二进制字节流收发传输；
4. 调用FileManager接口，完成文件分块发送、分块接收组装；
5. 预留v1协议打包、解包函数，交由protocol模块实现协议头解析；
6. 仅做数据搬运，不处理协议解析、文件校验、UI渲染等其他模块工作。

---

## 三、底层套接字工具 socket_tool（simple_socket.h / simple_socket.cpp）
### 公共类型与常量
```cpp
namespace socket_tool
{
    constexpr uint16_t PORT = 9090;
    using ByteBuf = std::vector<uint8_t>;
    using SocketFd = Windows:SOCKET / Linux:int;
    constexpr SocketFd INVALID_SOCK;
}
```
对外函数列表
1. bool initWinsock()
Windows 初始化 Winsock 库，Linux 直接返回 true。
2. SocketFd create_listener()
创建监听套接字，绑定 0.0.0.0:9090 并开启监听队列。
3. SocketFd accept_client(SocketFd listener)
阻塞等待单个客户端连接，返回新连接套接字句柄。
4. SocketFd connect_peer(const std::string& ip)
主动连接指定局域网 IP 的 9090 端口，返回连接句柄。
5. bool send_bytes(SocketFd fd, const ByteBuf& data)
发送整块二进制 ByteBuf 数据，发送成功返回 true。
6. ByteBuf recv_bytes(SocketFd fd, size_t max_recv = 81920)
阻塞接收一段二进制数据，连接断开 / 出错返回空容器。
7. void close_sock(SocketFd s)
跨平台安全关闭套接字（Windows:closesocket / Linux:close）。

业务传输接口：
1. wrap_protocol_v1 / unwrap_protocol_v1：协议扩展预留接口，当前原样透传二进制数据
2. send_file()：完整发送流程：读取文件→分块→序列化→套协议→网络发送
3. recv_file_loop()：循环收包、解包、反序列化块、写入会话、打印进度、文件组装落地

编译指令：
```
g++ -std=c++17 main.cpp transfer_logic.cpp simple_socket.cpp file_manager.cpp -o egg_trans.exe -lws2_32
```
操作使用步骤：

1. 两台设备连接同一个局域网 WiFi；
2. exe 程序同级目录手动新建空文件夹 downloads；
3. 测试文件放在 exe 同一目录，文件名禁止中文、空格；
4. 启动程序选择模式：
- 输入1：本机开启监听，等待对方连接
- 输入2：输入监听端 IPv4 地址，主动建立连接
- 连接成功后：
- 输入1：发送本地文件，输入文件名传输
- 输入2：进入接收模式，自动组装保存文件

## 四、模块对接关系：
1. 对接 Module3 FileManager
只调用模块公开 API，全程未修改 file_manager 源码。
调用接口：readAndChunkFile、serialize、deserialize、createTransferSession、addChunkToSession、getSessionProgress、isTransferComplete、completeTransfer。
2. 对接 Module4 UI
Qt 界面可直接调用 send_file、recv_file_loop 函数；本机自测使用回环地址 127.0.0.1。

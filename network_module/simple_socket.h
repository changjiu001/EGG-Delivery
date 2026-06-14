#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace socket_tool
{
    constexpr uint16_t PORT = 9090;
    using ByteBuf = std::vector<uint8_t>;

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
    using SocketFd = SOCKET;
    inline bool initWinsock()
    {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }
    inline void close_sock(SocketFd s) { closesocket(s); }
    constexpr SocketFd INVALID_SOCK = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
    using SocketFd = int;
    inline bool initWinsock() { return true; }
    inline void close_sock(SocketFd s) { ::close(s); }
    constexpr SocketFd INVALID_SOCK = -1;
#endif

    // 创建监听socket（本机等待别人连）
    SocketFd create_listener();
    // 阻塞等待一个客户端接入
    SocketFd accept_client(SocketFd listener);
    // 主动连接远端IP
    SocketFd connect_peer(const std::string& ip);
    // 整块发送二进制ByteBuf
    bool send_bytes(SocketFd fd, const ByteBuf& data);
    // 一次性接收一段数据（简易版，先不处理粘包，后续protocol处理）
    ByteBuf recv_bytes(SocketFd fd, size_t max_recv = 81920);
}
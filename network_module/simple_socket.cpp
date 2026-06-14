#include "simple_socket.h"

namespace socket_tool
{
    SocketFd create_listener()
    {
        if (!initWinsock()) return INVALID_SOCK;
        SocketFd sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCK) return INVALID_SOCK;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);

        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0)
        {
            close_sock(sock);
            return INVALID_SOCK;
        }
        listen(sock, 5);
        return sock;
    }

    SocketFd accept_client(SocketFd listener)
    {
        sockaddr_in peer{};
        int len = sizeof(peer);
        SocketFd cli = accept(listener, (sockaddr*)&peer, &len);
        return cli;
    }

    SocketFd connect_peer(const std::string& ip)
    {
        if (!initWinsock()) return INVALID_SOCK;
        SocketFd sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCK) return INVALID_SOCK;

        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(PORT);
        inet_pton(AF_INET, ip.c_str(), &target.sin_addr);

        if (connect(sock, (sockaddr*)&target, sizeof(target)) != 0)
        {
            close_sock(sock);
            return INVALID_SOCK;
        }
        return sock;
    }

    bool send_bytes(SocketFd fd, const ByteBuf& data)
    {
        if (data.empty()) return false;
        int ret = send(fd, (const char*)data.data(), (int)data.size(), 0);
        return ret > 0;
    }

    ByteBuf recv_bytes(SocketFd fd, size_t max_recv)
    {
        ByteBuf buf(max_recv, 0);
        int r = recv(fd, (char*)buf.data(), (int)max_recv, 0);
        if (r <= 0) return {};
        buf.resize(r);
        return buf;
    }
}
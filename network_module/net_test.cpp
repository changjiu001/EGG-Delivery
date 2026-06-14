#include "simple_socket.h"
#include "file_manager.h"
#include <iostream>
#include <string>

// 声明外部收发函数
extern bool send_file(socket_tool::SocketFd sock, const std::string& file_path);
extern void recv_file_loop(socket_tool::SocketFd sock);

int main()
{
    socket_tool::initWinsock();
    socket_tool::SocketFd sock = socket_tool::INVALID_SOCK;

    std::cout << "1 监听等待连接  |  2 主动连接对方\n";
    int mode;
    std::cin >> mode;

    if (mode == 1)
    {
        auto listener = socket_tool::create_listener();
        std::cout << "等待对方接入中...\n";
        sock = socket_tool::accept_client(listener);
        socket_tool::close_sock(listener);
    }
    else
    {
        std::string ip;
        std::cout << "输入对方局域网IP：";
        std::cin >> ip;
        sock = socket_tool::connect_peer(ip);
    }

    if (sock == socket_tool::INVALID_SOCK)
    {
        std::cout << "连接失败！\n";
        return -1;
    }
    std::cout << "网络连接建立成功\n";

    std::cout << "1=发送本地文件  |  2=等待接收文件\n";
    int op;
    std::cin >> op;
    if (op == 1)
    {
        std::string path;
        std::cout << "输入要发送的文件完整名称：";
        std::cin >> path;
        send_file(sock, path);
    }
    else
    {
        recv_file_loop(sock);
    }

    system("pause");
    socket_tool::close_sock(sock);
    return 0;
}
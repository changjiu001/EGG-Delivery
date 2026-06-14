#include "protocol.h"
#include <cstring>
#include <iostream>

namespace socket_tool
{
    // 全局在线设备列表
    std::map<std::string, DeviceInfo> g_online_devices;

    // ===================== 字节序转换（跨平台通用） =====================
    static uint16_t net_htons(uint16_t val) { return htons(val); }
    static uint32_t net_htonl(uint32_t val) { return htonl(val); }
    static uint16_t net_ntohs(uint16_t val) { return ntohs(val); }
    static uint32_t net_ntohl(uint32_t val) { return ntohl(val); }

    // ===================== 通用外层封包 =====================
    ByteBuf encode_outer_packet(PacketType type, const ByteBuf& body)
    {
        ByteBuf pkg;
        pkg.reserve(OUTER_HEAD_LEN + body.size());

        // 1. 填充外层包头
        OuterHeader head{};
        head.pkg_type = static_cast<uint8_t>(type);
        head.body_len = net_htonl(static_cast<uint32_t>(body.size()));

        // 拷贝包头到缓冲区
        const uint8_t* head_ptr = reinterpret_cast<const uint8_t*>(&head);
        pkg.insert(pkg.end(), head_ptr, head_ptr + OUTER_HEAD_LEN);

        // 2. 追加包体数据
        pkg.insert(pkg.end(), body.begin(), body.end());
        return pkg;
    }

    // ===================== 通用外层解包 =====================
    bool decode_outer_packet(const ByteBuf& pkg, PacketType& out_type, ByteBuf& out_body)
    {
        // 数据长度不足包头，直接丢弃
        if (pkg.size() < OUTER_HEAD_LEN)
            return false;

        // 解析包头
        OuterHeader head{};
        std::memcpy(&head, pkg.data(), OUTER_HEAD_LEN);
        out_type = static_cast<PacketType>(head.pkg_type);
        uint32_t body_len = net_ntohl(head.body_len);

        // 校验数据完整性
        if (pkg.size() != OUTER_HEAD_LEN + body_len)
            return false;

        // 截取包体
        out_body.assign(pkg.begin() + OUTER_HEAD_LEN, pkg.end());
        return true;
    }

    // ===================== 业务快捷封包实现 =====================
    ByteBuf encode_file_packet(const ByteBuf& file_chunk_data)
    {
        return encode_outer_packet(PacketType::FILE_DATA, file_chunk_data);
    }

    ByteBuf encode_chat_packet(const std::string& sender, const std::string& content)
    {
        ChatMsgBody msg{};
        std::memset(&msg, 0, sizeof(msg));
        std::strncpy(msg.sender, sender.c_str(), sizeof(msg.sender) - 1);
        std::strncpy(msg.content, content.c_str(), sizeof(msg.content) - 1);

        ByteBuf body(reinterpret_cast<uint8_t*>(&msg), reinterpret_cast<uint8_t*>(&msg) + sizeof(msg));
        return encode_outer_packet(PacketType::CHAT_MSG, body);
    }

    ByteBuf encode_online_broadcast(const std::string& local_ip, const std::string& name)
    {
        DeviceInfo dev{};
        std::memset(&dev, 0, sizeof(dev));
        std::strncpy(dev.ip, local_ip.c_str(), sizeof(dev.ip) - 1);
        std::strncpy(dev.name, name.c_str(), sizeof(dev.name) - 1);

        ByteBuf body(reinterpret_cast<uint8_t*>(&dev), reinterpret_cast<uint8_t*>(&dev) + sizeof(dev));
        return encode_outer_packet(PacketType::DEV_ONLINE, body);
    }

    ByteBuf encode_offline_broadcast(const std::string& local_ip, const std::string& name)
    {
        DeviceInfo dev{};
        std::memset(&dev, 0, sizeof(dev));
        std::strncpy(dev.ip, local_ip.c_str(), sizeof(dev.ip) - 1);
        std::strncpy(dev.name, name.c_str(), sizeof(dev.name) - 1);

        ByteBuf body(reinterpret_cast<uint8_t*>(&dev), reinterpret_cast<uint8_t*>(&dev) + sizeof(dev));
        return encode_outer_packet(PacketType::DEV_OFFLINE, body);
    }

    // ===================== UDP 局域网设备发现 =====================
    SocketFd start_discovery_listener()
    {
        // Windows 初始化Winsock（Linux空实现）
        initWinsock();

        // 创建UDP套接字
        SocketFd udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_sock == INVALID_SOCK)
        {
            std::cerr << "[协议模块] 创建UDP监听套接字失败" << std::endl;
            return INVALID_SOCK;
        }

        // 开启广播权限
        int broadcast = 1;
        setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

        // 绑定本机端口
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = net_htons(DISCOVERY_UDP_PORT);
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(udp_sock, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0)
        {
            std::cerr << "[协议模块] UDP端口绑定失败" << std::endl;
            close_sock(udp_sock);
            return INVALID_SOCK;
        }

        std::cout << "[协议模块] 局域网发现服务已启动，端口: " << DISCOVERY_UDP_PORT << std::endl;
        return udp_sock;
    }

    int send_online_broadcast(SocketFd udp_sock, const std::string& local_ip, const std::string& name)
    {
        if (udp_sock == INVALID_SOCK)
            return -1;

        ByteBuf pkg = encode_online_broadcast(local_ip, name);

        // 全网段广播地址 255.255.255.255
        sockaddr_in broadcast_addr{};
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = net_htons(DISCOVERY_UDP_PORT);
        broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        return sendto(udp_sock, reinterpret_cast<const char*>(pkg.data()), (int)pkg.size(),
            0, reinterpret_cast<sockaddr*>(&broadcast_addr), sizeof(broadcast_addr));
    }

    void recv_broadcast_loop(SocketFd udp_sock)
    {
        if (udp_sock == INVALID_SOCK)
            return;

        uint8_t recv_buf[PROTO_BUF_MAX] = { 0 };
        sockaddr_in remote_addr{};
#ifdef _WIN32
        int addr_len = sizeof(remote_addr);
#else
        socklen_t addr_len = sizeof(remote_addr);
#endif

        while (true)
        {
            int recv_len = recvfrom(udp_sock, reinterpret_cast<char*>(recv_buf), PROTO_BUF_MAX,
                0, reinterpret_cast<sockaddr*>(&remote_addr), &addr_len);
            if (recv_len <= 0)
                continue;

            ByteBuf pkg(recv_buf, recv_buf + recv_len);
            PacketType type;
            ByteBuf body;
            if (!decode_outer_packet(pkg, type, body))
                continue;

            // 处理上线包
            if (type == PacketType::DEV_ONLINE && body.size() == sizeof(DeviceInfo))
            {
                DeviceInfo dev{};
                std::memcpy(&dev, body.data(), sizeof(DeviceInfo));
                std::string ip(dev.ip);
                g_online_devices[ip] = dev;
                std::cout << "[发现设备] 昵称: " << dev.name << "  IP: " << ip << std::endl;
            }
            // 处理下线包
            else if (type == PacketType::DEV_OFFLINE && body.size() == sizeof(DeviceInfo))
            {
                DeviceInfo dev{};
                std::memcpy(&dev, body.data(), sizeof(DeviceInfo));
                std::string ip(dev.ip);
                g_online_devices.erase(ip);
                std::cout << "[设备下线] 昵称: " << dev.name << "  IP: " << ip << std::endl;
            }
        }
    }

    const std::map<std::string, DeviceInfo>& get_online_devices()
    {
        return g_online_devices;
    }

} // namespace socket_tool
#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <map>

// 引入项目基础头文件（跨平台Socket、ByteBuf）
#include "../../simple_socket.h"
#include "../../file_manager.h"

namespace socket_tool
{
    // ===================== 全局宏定义 =====================
    constexpr uint16_t DISCOVERY_UDP_PORT = 8888;  // 局域网发现UDP端口
    constexpr size_t PROTO_BUF_MAX = 81920;        // 缓冲区大小（和网络层对齐）
    constexpr size_t OUTER_HEAD_LEN = 5;           // 外层协议头固定5字节

    // ===================== 数据包类型枚举 =====================
    enum class PacketType : uint8_t
    {
        FILE_DATA = 0,
        CHAT_MSG = 1,
        DEV_ONLINE = 2,
        DEV_OFFLINE = 3,
        HEARTBEAT = 4
    };

    // ===================== 外层协议头结构体 =====================
    typedef struct
    {
        uint8_t  pkg_type;    // 1B 包类型
        uint32_t body_len;    // 4B 包体长度（网络字节序）
    } OuterHeader;

    // ===================== 业务包体结构体 =====================
    // 聊天消息包体
    typedef struct
    {
        char sender[32];   // 发送者昵称
        char content[512]; // 聊天内容
    } ChatMsgBody;

    // 局域网设备信息（上下线广播用）
    typedef struct
    {
        char ip[16];       // 设备IP
        char name[32];     // 设备昵称
    } DeviceInfo;

    // ===================== 全局在线设备列表 =====================
    // key: IP地址  value: 设备信息
    extern std::map<std::string, DeviceInfo> g_online_devices;

    // ===================== 核心：通用封包/解包 =====================
    /**
     * @brief 外层统一封包：包头 + 原始数据
     * @param type 包类型
     * @param body 原始包体数据
     * @return 组装完成的完整数据包
     */
    ByteBuf encode_outer_packet(PacketType type, const ByteBuf& body);

    /**
     * @brief 外层统一解包：剥离包头，取出包体
     * @param pkg 完整网络数据包
     * @param out_type 输出：包类型
     * @param out_body 输出：原始包体
     * @return 解析成功返回true
     */
    bool decode_outer_packet(const ByteBuf& pkg, PacketType& out_type, ByteBuf& out_body);

    // ===================== 业务快捷封包（对外接口） =====================
    // 1. 文件数据包封装（对接原有FileChunk）
    ByteBuf encode_file_packet(const ByteBuf& file_chunk_data);

    // 2. 聊天消息封装
    ByteBuf encode_chat_packet(const std::string& sender, const std::string& content);

    // 3. 设备上线广播包（UDP）
    ByteBuf encode_online_broadcast(const std::string& local_ip, const std::string& name);

    // 4. 设备下线广播包（UDP）
    ByteBuf encode_offline_broadcast(const std::string& local_ip, const std::string& name);

    // ===================== 局域网UDP设备发现模块 =====================
    // 启动UDP广播监听套接字
    SocketFd start_discovery_listener();

    // 发送全网段上线广播
    int send_online_broadcast(SocketFd udp_sock, const std::string& local_ip, const std::string& name);

    // 循环接收广播包、更新在线设备列表
    void recv_broadcast_loop(SocketFd udp_sock);

    // 获取在线设备列表（供UI/主程序调用）
    const std::map<std::string, DeviceInfo>& get_online_devices();

} // namespace socket_tool
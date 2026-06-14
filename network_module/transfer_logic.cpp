#include "simple_socket.h"
#include "file_manager.h"
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>

// 预留协议包裹函数，后续protocol模块填充
socket_tool::ByteBuf wrap_protocol_v1(const socket_tool::ByteBuf& chunk_data)
{
    return chunk_data;
}

socket_tool::ByteBuf unwrap_protocol_v1(const socket_tool::ByteBuf& pkg)
{
    return pkg;
}

// 发送文件
bool send_file(socket_tool::SocketFd sock, const std::string& file_path)
{
    FileManager fm("./downloads");
    auto chunk_list = fm.readAndChunkFile(file_path);

    if (chunk_list.empty())
    {
        std::cout << "文件读取失败，检查路径是否正确" << std::endl;
        return false;
    }

    std::cout << "文件拆分总块数：" << chunk_list.size() << std::endl;

    for (auto& chunk : chunk_list)
    {
        auto raw = chunk.serialize();
        auto pkg = wrap_protocol_v1(raw);
        bool ok = socket_tool::send_bytes(sock, pkg);
        if (!ok)
        {
            std::cout << "块" << chunk.chunk_id << "发送失败" << std::endl;
            return false;
        }
        std::cout << "已发送块：" << chunk.chunk_id << std::endl;
    }
    std::cout << "全部发送完成！" << std::endl;
    return true;
}

// 循环接收文件块
void recv_file_loop(socket_tool::SocketFd sock)
{
    FileManager fm("./downloads");
    std::unordered_map<std::string, bool> session_map;
    std::cout << "等待接收文件..." << std::endl;

    while (true)
    {
        auto pkg = socket_tool::recv_bytes(sock);
        if (pkg.empty())
        {
            std::cout << "连接断开，停止接收" << std::endl;
            break;
        }
        auto raw_chunk = unwrap_protocol_v1(pkg);
        FileChunk chunk;
        if (!chunk.deserialize(raw_chunk))
        {
            std::cout << "数据包校验失败，丢弃" << std::endl;
            continue;
        }

        std::string hash_id(chunk.file_hash.begin(), chunk.file_hash.end());
        auto it = session_map.find(hash_id);
        if (it == session_map.end())
        {
            std::string save_name = hash_id + "_out.bin";
            fm.createTransferSession(chunk.file_hash, save_name, chunk.total_chunks);
            session_map[hash_id] = true;
        }

        fm.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data);
        int progress = fm.getSessionProgress(chunk.file_hash);
        std::cout << "当前文件进度：" << progress << "%" << std::endl;

        if (fm.isTransferComplete(chunk.file_hash))
        {
            fm.completeTransfer(chunk.file_hash);
            std::cout << "文件接收完毕，保存在downloads文件夹" << std::endl;
            session_map.erase(hash_id);
        }
    }
}
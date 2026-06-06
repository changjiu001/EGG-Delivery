#include "file_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * 示例1：读取文件并分块
 * 展示文件分块的基本流程
 */
void example_read_and_chunk() {
    std::cout << "\n=== 示例1：文件读取与分块 ===" << std::endl;
    
    FileManager fm("./downloads");
    
    // 假设存在一个测试文件
    std::string test_file = "test_file.bin";
    
    // 创建一个测试文件（10MB）
    {
        std::ofstream out(test_file, std::ios::binary);
        for (int i = 0; i < 10 * 1024; ++i) {
            char buffer[1024];
            for (int j = 0; j < 1024; ++j) {
                buffer[j] = (i + j) % 256;
            }
            out.write(buffer, 1024);
        }
        out.close();
    }
    
    // 读取并分块（每块64KB）
    auto chunks = fm.readAndChunkFile(test_file, 64 * 1024);
    
    std::cout << "Total chunks generated: " << chunks.size() << std::endl;
    for (size_t i = 0; i < std::min(size_t(3), chunks.size()); ++i) {
        std::cout << "  Chunk " << i << ": ID=" << chunks[i].chunk_id
                  << ", Size=" << chunks[i].data_size
                  << ", Total=" << chunks[i].total_chunks << std::endl;
    }
}

/**
 * 示例2：模拟文件接收过程
 * 展示接收方如何处理分块并组装文件
 */
void example_receive_file() {
    std::cout << "\n=== 示例2：文件接收与组装 ===" << std::endl;
    
    FileManager fm("./downloads");
    
    // 模拟发送方：读取文件
    std::string source_file = "source.bin";
    {
        std::ofstream out(source_file, std::ios::binary);
        std::string content = "Hello, LAN Chat! This is a test file for chunk transfer.";
        out.write(content.c_str(), content.size());
        out.close();
    }
    
    auto chunks = fm.readAndChunkFile(source_file, 16); // 小块便于演示
    
    // 模拟接收方：创建传输会话
    std::string file_hash = FileManager::calculateFileHash(source_file);
    fm.createTransferSession(file_hash, "received_file.bin", chunks.size());
    
    // 模拟分块接收（可能乱序）
    std::cout << "\nReceiving chunks (simulated)..." << std::endl;
    for (const auto& chunk : chunks) {
        fm.addChunkToSession(file_hash, chunk.chunk_id, chunk.data);
        int32_t progress = fm.getSessionProgress(file_hash);
        std::cout << "Progress: " << progress << "%" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 完成传输
    if (fm.isTransferComplete(file_hash)) {
        std::cout << "All chunks received! Assembling file..." << std::endl;
        fm.completeTransfer(file_hash);
        std::cout << "File saved to: " << fm.getDownloadDirectory() << "/received_file.bin" << std::endl;
    }
}

/**
 * 示例3：处理多个并发文件传输
 * 展示文件管理器的并发处理能力
 */
void example_concurrent_transfers() {
    std::cout << "\n=== 示例3：并发文件传输 ===" << std::endl;
    
    FileManager fm("./downloads");
    
    // 模拟3个并发文件传输
    for (int file_num = 1; file_num <= 3; ++file_num) {
        std::string file_name = "file_" + std::to_string(file_num) + ".bin";
        
        // 创建测试文件
        {
            std::ofstream out(file_name, std::ios::binary);
            std::string content = "Content of file " + std::to_string(file_num);
            for (int i = 0; i < 100; ++i) {
                out.write(content.c_str(), content.size());
            }
            out.close();
        }
        
        // 开始文件传输
        auto chunks = fm.readAndChunkFile(file_name, 32);
        std::string hash = FileManager::calculateFileHash(file_name);
        fm.createTransferSession(hash, "received_" + file_name, chunks.size());
        
        // 模拟接收部分分块
        for (size_t i = 0; i < chunks.size() / 2; ++i) {
            fm.addChunkToSession(hash, chunks[i].chunk_id, chunks[i].data);
        }
    }
    
    // 显示活跃会话
    auto active_sessions = fm.getActiveSessions();
    std::cout << "\nActive transfer sessions: " << active_sessions.size() << std::endl;
    for (const auto& session : active_sessions) {
        int32_t progress = fm.getSessionProgress(session);
        std::cout << "  Session " << session << ": " << progress << "%" << std::endl;
    }
}

/**
 * 示例4：文件分块的序列化与反序列化
 * 展示网络传输前的数据格式化
 */
void example_chunk_serialization() {
    std::cout << "\n=== 示例4：分块序列化与反序列化 ===" << std::endl;
    
    // 创建一个分块
    FileChunk chunk;
    chunk.chunk_id = 5;
    chunk.total_chunks = 100;
    chunk.file_hash = "abc123def456";
    chunk.data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    chunk.data_size = 5;
    
    std::cout << "Original chunk:" << std::endl;
    std::cout << "  ID: " << chunk.chunk_id << "/" << chunk.total_chunks << std::endl;
    std::cout << "  Hash: " << chunk.file_hash << std::endl;
    std::cout << "  Data size: " << chunk.data_size << std::endl;
    
    // 序列化
    auto serialized = chunk.serialize();
    std::cout << "\nSerialized size: " << serialized.size() << " bytes" << std::endl;
    
    // 反序列化
    FileChunk deserialized;
    if (deserialized.deserialize(serialized)) {
        std::cout << "\nDeserialized chunk:" << std::endl;
        std::cout << "  ID: " << deserialized.chunk_id << "/" << deserialized.total_chunks << std::endl;
        std::cout << "  Hash: " << deserialized.file_hash << std::endl;
        std::cout << "  Data size: " << deserialized.data_size << std::endl;
        std::cout << "  Match: " << (chunk.file_hash == deserialized.file_hash ? "YES" : "NO") << std::endl;
    }
}

int main() {
    std::cout << "=== LAN Chat File Manager - Demo ===" << std::endl;
    
    try {
        example_read_and_chunk();
        example_receive_file();
        example_concurrent_transfers();
        example_chunk_serialization();
        
        std::cout << "\n=== All demos completed ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

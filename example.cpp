#include "file_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

// ============================================================================
// 示例 1：SHA256 哈希演示
// ============================================================================
void example_sha256() {
    std::cout << "\n=== 示例 1：SHA256 哈希 ===" << std::endl;

    // 字符串哈希
    SHA256 sha;
    sha.update("The quick brown fox jumps over the lazy dog");
    std::cout << "SHA256(message) = " << sha.digest() << std::endl;

    // 文件哈希
    std::string test_file = "test_file.bin";
    {
        std::ofstream out(test_file, std::ios::binary);
        out << "Hello, EGG-Delivery! This file is used for SHA256 demo.";
    }
    std::cout << "SHA256(file)   = " << SHA256::hashFile(test_file) << std::endl;
}

// ============================================================================
// 示例 2：文件分块与协议输出
// ============================================================================
void example_chunking() {
    std::cout << "\n=== 示例 2：文件读取与分块 ===" << std::endl;

    // 创建一个 ~1MB 测试文件
    std::string test_file = "demo_file.bin";
    {
        std::ofstream out(test_file, std::ios::binary);
        for (int i = 0; i < 1024; ++i) {
            char buf[1024];
            for (int j = 0; j < 1024; ++j) buf[j] = (i + j) % 256;
            out.write(buf, 1024);
        }
    }

    FileManager fm("./downloads");
    auto chunks = fm.readAndChunkFile(test_file, 64 * 1024);

    std::cout << "\n生成 " << chunks.size() << " 个分块\n" << std::endl;

    // 显示前 3 块信息
    for (size_t i = 0; i < std::min(size_t(3), chunks.size()); ++i) {
        auto wire = chunks[i].serialize();
        std::cout << "  Chunk " << i << ":\n"
                  << "    ID=" << chunks[i].chunk_id
                  << " / Total=" << chunks[i].total_chunks << "\n"
                  << "    Data=" << chunks[i].data.size() << " bytes\n"
                  << "    Wire=" << wire.size() << " bytes (含协议头 + CRC32)\n"
                  << "    Hash=" << chunks[i].file_hash.substr(0, 16) << "...\n"
                  << std::endl;
    }
}

// ============================================================================
// 示例 3：文件接收与组装（含 CRC32 校验）
// ============================================================================
void example_receive() {
    std::cout << "\n=== 示例 3：文件接收与组装 ===" << std::endl;

    // 准备源文件
    std::string source = "source_demo.bin";
    {
        std::ofstream out(source, std::ios::binary);
        std::string content = "Welcome to EGG-Delivery! "
                              "This is a demonstration of chunked file transfer.";
        out.write(content.data(), content.size());
    }

    // 发送端：读取 + 分块
    FileManager sender("./downloads");
    auto chunks = sender.readAndChunkFile(source, 16);  // 小块，便于演示乱序

    std::string file_hash = SHA256::hashFile(source);

    // 接收端
    FileManager receiver("./downloads");
    receiver.createTransferSession(file_hash, "received_demo.bin",
                                   static_cast<uint32_t>(chunks.size()));

    // 模拟乱序接收
    std::cout << "\n模拟乱序接收 " << chunks.size() << " 个分块..." << std::endl;
    for (size_t i = chunks.size(); i > 0; --i) {
        size_t idx = i - 1;
        auto wire = chunks[idx].serialize();

        // 网络层收到 wire 后反序列化
        FileChunk arrived;
        if (arrived.deserialize(wire)) {
            receiver.addChunkToSession(file_hash, arrived.chunk_id, arrived.data);
            std::cout << "  ← Chunk " << arrived.chunk_id
                      << " (CRC OK), Progress: "
                      << receiver.getSessionProgress(file_hash) << "%"
                      << std::endl;
        } else {
            std::cout << "  ✗ Chunk corrupted, discarded" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 完成传输
    if (receiver.isTransferComplete(file_hash)) {
        std::cout << "\n✓ 所有分块到齐，开始组装..." << std::endl;
        receiver.completeTransfer(file_hash);
        std::cout << "✓ 文件已保存到: " << receiver.getDownloadDirectory()
                  << "/received_demo.bin" << std::endl;
    }
}

// ============================================================================
// 示例 4：并发传输 + UI 轮询
// ============================================================================
void example_concurrent() {
    std::cout << "\n=== 示例 4：并发传输 + 进度轮询 ===" << std::endl;

    FileManager fm("./downloads");

    // 创建 3 个并发会话
    fm.createTransferSession("hash_a", "movie.mp4", 200);
    fm.createTransferSession("hash_b", "photo.zip", 50);
    fm.createTransferSession("hash_c", "notes.txt", 5);

    // 模拟传输中...
    for (int round = 0; round < 3; ++round) {
        fm.addChunkToSession("hash_a", round, std::vector<uint8_t>(64 * 1024));
        fm.addChunkToSession("hash_b", round, std::vector<uint8_t>(64 * 1024));
        fm.addChunkToSession("hash_c", round, std::vector<uint8_t>(100));
    }

    // UI 层这样轮询
    std::cout << "\n活跃传输：" << std::endl;
    auto sessions = fm.getActiveSessions();
    for (const auto& hash : sessions) {
        int pct = fm.getSessionProgress(hash);
        std::cout << "  " << hash << "  [";
        int bars = pct / 5;
        for (int i = 0; i < 20; ++i) std::cout << (i < bars ? "█" : "░");
        std::cout << "] " << pct << "%" << std::endl;
    }

    std::cout << "\n总活跃会话: " << fm.sessionCount() << std::endl;
}

// ============================================================================
// 示例 5：空闲清理
// ============================================================================
void example_cleanup() {
    std::cout << "\n=== 示例 5：空闲会话清理 ===" << std::endl;

    FileManager fm("./downloads");

    fm.createTransferSession("stale_hash", "stale.bin", 100);

    std::cout << "创建后会话数: " << fm.sessionCount() << std::endl;

    // 等待 1 秒...
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 用 0 秒超时清理所有
    size_t removed = fm.cleanupIdleSessions(0);
    std::cout << "清理了 " << removed << " 个空闲会话" << std::endl;
    std::cout << "剩余会话数: " << fm.sessionCount() << std::endl;
}

// ============================================================================
int main() {
    std::cout << "╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║   EGG-Delivery  Module 3  —  演示程序    ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;

    try {
        example_sha256();
        example_chunking();
        example_receive();
        example_concurrent();
        example_cleanup();

        std::cout << "\n=== 全部示例完成 ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

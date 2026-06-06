#include "file_manager.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class FileManagerTest {
private:
    std::string test_dir = "./test_data";

public:
    FileManagerTest() {
        fs::create_directories(test_dir);
    }

    ~FileManagerTest() {
        fs::remove_all(test_dir);
    }

    // ========================================================================
    bool test_sha256_known_vector() {
        std::cout << "\n[TEST 1] SHA256 known-answer test..." << std::endl;

        // NIST test vector: SHA256("abc")
        SHA256 sha;
        sha.update("abc", 3);
        std::string d = sha.digest();

        const std::string expected = "ba7816bf8f01cfea414140de5dae2223"
                                     "b00361a396177a9cb410ff61f20015ad";
        assert(d == expected);

        std::cout << "✓ SHA256(\"abc\") = " << d << std::endl;
        return true;
    }

    // ========================================================================
    bool test_sha256_empty() {
        std::cout << "\n[TEST 2] SHA256 empty string..." << std::endl;

        SHA256 sha;
        sha.update("", 0);
        std::string d = sha.digest();

        const std::string expected = "e3b0c44298fc1c149afbf4c8996fb924"
                                     "27ae41e4649b934ca495991b7852b855";
        assert(d == expected);

        std::cout << "✓ SHA256(\"\") = " << d << std::endl;
        return true;
    }

    // ========================================================================
    bool test_sha256_file() {
        std::cout << "\n[TEST 3] SHA256 file hashing..." << std::endl;

        std::string path = test_dir + "/sha_test.bin";
        {
            std::ofstream out(path, std::ios::binary);
            out.write("Hello, EGG-Delivery!", 20);
        }

        std::string h1 = SHA256::hashFile(path);
        std::string h2 = SHA256::hashFile(path);
        assert(!h1.empty());
        assert(h1 == h2);   // 两次计算结果一致

        // 改文件，hash 应变
        {
            std::ofstream out(path, std::ios::binary);
            out.write("Hello, EGG-Delivery!!", 21);
        }
        std::string h3 = SHA256::hashFile(path);
        assert(h1 != h3);

        std::cout << "✓ SHA256 file hash consistent & sensitive to change" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_crc32() {
        std::cout << "\n[TEST 4] CRC32 checksum..." << std::endl;

        const char* msg = "123456789";
        uint32_t c = crc32(msg, 9);

        // 标准 CRC32("123456789") = 0xCBF43926
        assert(c == 0xCBF43926);

        // 相同数据相同 CRC
        uint32_t c2 = crc32(msg, 9);
        assert(c == c2);

        // 不同数据不同 CRC
        uint32_t c3 = crc32("123456788", 9);
        assert(c != c3);

        std::cout << "✓ CRC32(\"123456789\") = 0x"
                  << std::hex << c << std::dec << std::endl;
        return true;
    }

    // ========================================================================
    bool test_chunk_serialization_crc() {
        std::cout << "\n[TEST 5] Chunk serialization with CRC32..." << std::endl;

        FileChunk original;
        original.chunk_id     = 7;
        original.total_chunks = 30;
        original.file_hash    = SHA256::hashData("payload", 7);
        original.data         = {0x01, 0x02, 0x03, 0x04, 0x05};
        original.data_size    = 5;

        auto wire = original.serialize();
        assert(wire.size() > 25);  // header + hash + data

        // 正常反序列化
        FileChunk restored;
        assert(restored.deserialize(wire));
        assert(restored.chunk_id     == original.chunk_id);
        assert(restored.total_chunks == original.total_chunks);
        assert(restored.file_hash    == original.file_hash);
        assert(restored.data         == original.data);

        // 损坏数据应被 CRC32 检测
        auto corrupted = wire;
        corrupted.back() ^= 0xFF;   // 翻转最后一个字节
        FileChunk bad;
        assert(!bad.deserialize(corrupted));  // CRC32 不匹配

        // 错误魔数应被拒绝
        auto bad_magic = wire;
        bad_magic[0] = 0x00;
        FileChunk junk;
        assert(!junk.deserialize(bad_magic));

        std::cout << "✓ CRC32 detects corruption, magic rejects garbage" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_transfer_session_basics() {
        std::cout << "\n[TEST 6] TransferSession — sequential & unordered..." << std::endl;

        FileTransferSession session("hash_abc", test_dir + "/seq.bin", 5);

        // 顺序添加
        for (uint32_t i = 0; i < 5; ++i) {
            std::vector<uint8_t> d(100, static_cast<uint8_t>(i));
            assert(session.addChunk(i, d));
        }
        assert(session.isComplete());
        assert(session.getProgressPercentage() == 100);
        assert(session.getMissingChunks().empty());

        // 乱序会话
        FileTransferSession session2("hash_xyz", test_dir + "/rnd.bin", 5);
        uint32_t order[] = {2, 0, 4, 1, 3};
        for (uint32_t idx : order) {
            std::vector<uint8_t> d(50);
            assert(session2.addChunk(idx, d));
        }
        assert(session2.isComplete());
        assert(session2.getReceivedChunkCount() == 5);

        std::cout << "✓ Both sequential and unordered reception work" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_missing_chunks() {
        std::cout << "\n[TEST 7] getMissingChunks..." << std::endl;

        FileTransferSession session("hash_m", test_dir + "/m.bin", 10);

        // 只接收偶数块
        for (uint32_t i = 0; i < 10; i += 2) {
            session.addChunk(i, {0x00});
        }

        auto missing = session.getMissingChunks();
        assert(missing.size() == 5);
        for (auto id : missing) {
            assert(id % 2 == 1);  // 全是奇数
        }

        assert(session.getProgressPercentage() == 50);

        std::cout << "✓ Missing chunks: [";
        for (auto id : missing) std::cout << id << " ";
        std::cout << "]" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_idle_tracking() {
        std::cout << "\n[TEST 8] Idle time tracking & cleanup..." << std::endl;

        FileManager fm(test_dir + "/idle_test");

        fm.createTransferSession("file_a", "a.bin", 10);
        fm.createTransferSession("file_b", "b.bin", 20);

        assert(fm.sessionCount() == 2);

        // 立即清理不应删任何会话（空闲 < 3600s）
        size_t removed = fm.cleanupIdleSessions(3600);
        assert(removed == 0);
        assert(fm.sessionCount() == 2);

        // 用极短超时（0 秒）强制清理所有
        removed = fm.cleanupIdleSessions(0);
        assert(removed == 2);
        assert(fm.sessionCount() == 0);

        std::cout << "✓ Idle cleanup works: removed " << removed << " sessions" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_full_transfer_workflow() {
        std::cout << "\n[TEST 9] End-to-end file transfer workflow..." << std::endl;

        // 创建源文件
        std::string source = test_dir + "/source_e2e.bin";
        std::string content(1024 * 100, 'X');  // 100 KB
        for (size_t i = 0; i < content.size(); ++i) {
            content[i] = static_cast<char>((i % 256));
        }
        {
            std::ofstream out(source, std::ios::binary);
            out.write(content.data(), content.size());
        }

        // 发送端
        FileManager sender(test_dir + "/sender");
        auto chunks = sender.readAndChunkFile(source, 4096);  // 4KB 分块
        assert(!chunks.empty());

        std::string file_hash = SHA256::hashFile(source);
        assert(chunks[0].file_hash == file_hash);

        // 接收端
        FileManager receiver(test_dir + "/receiver");
        receiver.createTransferSession(file_hash, "received.bin",
                                       static_cast<uint32_t>(chunks.size()));

        // 模拟乱序传输
        for (size_t i = chunks.size(); i > 0; --i) {
            size_t idx = i - 1;
            receiver.addChunkToSession(file_hash,
                                       chunks[idx].chunk_id,
                                       chunks[idx].data);
        }

        assert(receiver.isTransferComplete(file_hash));
        assert(receiver.getSessionProgress(file_hash) == 100);

        // 组装
        assert(receiver.completeTransfer(file_hash));
        assert(receiver.sessionCount() == 0);  // 完成后自动清理

        // 验证文件内容
        std::string received_path = test_dir + "/receiver/received.bin";
        std::ifstream in(received_path, std::ios::binary);
        std::string received_content((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());

        assert(received_content == content);
        assert(SHA256::hashFile(received_path) == file_hash);

        std::cout << "✓ End-to-end: " << chunks.size() << " chunks, SHA256 verified"
                  << std::endl;
        return true;
    }

    // ========================================================================
    bool test_cancel_transfer() {
        std::cout << "\n[TEST 10] Cancel transfer..." << std::endl;

        FileManager fm(test_dir + "/cancel_test");
        fm.createTransferSession("cancel_me", "test.bin", 10);

        for (uint32_t i = 0; i < 3; ++i) {
            fm.addChunkToSession("cancel_me", i, {0x00});
        }

        assert(fm.getSessionProgress("cancel_me") == 30);
        assert(fm.cancelTransfer("cancel_me"));
        assert(fm.getActiveSessions().empty());
        assert(fm.getSessionProgress("cancel_me") == -1);

        std::cout << "✓ Transfer cancelled and session removed" << std::endl;
        return true;
    }

    // ========================================================================
    bool test_large_file() {
        std::cout << "\n[TEST 11] Large file handling (10 MB)..." << std::endl;

        std::string large = test_dir + "/large_10mb.bin";
        {
            std::ofstream out(large, std::ios::binary);
            for (int i = 0; i < 10240; ++i) {
                char buf[1024];
                for (int j = 0; j < 1024; ++j) buf[j] = (i + j) % 256;
                out.write(buf, 1024);
            }
        }

        FileManager fm(test_dir + "/large_test");
        auto chunks = fm.readAndChunkFile(large, 256 * 1024);  // 256 KB 块

        assert(chunks.size() == 40);  // 10 MB / 256 KB = 40
        assert(!chunks[0].file_hash.empty());

        // 验证 hash 一致性
        std::string h1 = SHA256::hashFile(large);
        assert(chunks[0].file_hash == h1);

        std::cout << "✓ 10 MB → " << chunks.size() << " chunks, hash=" << h1 << std::endl;
        return true;
    }

    // ========================================================================
    bool test_multi_session() {
        std::cout << "\n[TEST 12] Multiple concurrent sessions..." << std::endl;

        FileManager fm(test_dir + "/multi");

        fm.createTransferSession("h1", "f1.bin", 10);
        fm.createTransferSession("h2", "f2.bin", 20);
        fm.createTransferSession("h3", "f3.bin", 30);

        assert(fm.sessionCount() == 3);

        // 各自独立进度
        fm.addChunkToSession("h1", 0, {0x00});
        fm.addChunkToSession("h1", 1, {0x00});
        fm.addChunkToSession("h2", 0, {0x00});

        assert(fm.getSessionProgress("h1") == 20);  // 2/10
        assert(fm.getSessionProgress("h2") == 5);   // 1/20
        assert(fm.getSessionProgress("h3") == 0);   // 0/30
        assert(fm.getSessionProgress("h99") == -1); // 不存在

        auto active = fm.getActiveSessions();
        assert(active.size() == 3);

        std::cout << "✓ 3 concurrent sessions, independent progress" << std::endl;
        return true;
    }

    // ========================================================================
    bool run_all() {
        std::cout << "========================================" << std::endl;
        std::cout << "  FileManager Unit Tests (v2)" << std::endl;
        std::cout << "========================================" << std::endl;

        try {
            test_sha256_known_vector();
            test_sha256_empty();
            test_sha256_file();
            test_crc32();
            test_chunk_serialization_crc();
            test_transfer_session_basics();
            test_missing_chunks();
            test_idle_tracking();
            test_full_transfer_workflow();
            test_cancel_transfer();
            test_large_file();
            test_multi_session();

            std::cout << "\n========================================" << std::endl;
            std::cout << "  ✓ All 12 tests passed!" << std::endl;
            std::cout << "========================================" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
            return false;
        }
    }
};

int main() {
    FileManagerTest tester;
    return tester.run_all() ? 0 : 1;
}

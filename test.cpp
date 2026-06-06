#include "file_manager.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

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
    
    // 测试1：文件读取和分块
    bool test_read_and_chunk() {
        std::cout << "\n[TEST 1] Reading and chunking file..." << std::endl;
        
        // 创建测试文件 (1MB)
        std::string test_file = test_dir + "/test_1mb.bin";
        {
            std::ofstream out(test_file, std::ios::binary);
            for (int i = 0; i < 1024; ++i) {
                char buffer[1024];
                for (int j = 0; j < 1024; ++j) {
                    buffer[j] = (i + j) % 256;
                }
                out.write(buffer, 1024);
            }
        }
        
        FileManager fm(test_dir + "/downloads");
        auto chunks = fm.readAndChunkFile(test_file, 64 * 1024);
        
        // 验证
        assert(chunks.size() == 16);  // 1MB / 64KB = 16块
        assert(chunks[0].chunk_id == 0);
        assert(chunks[0].total_chunks == 16);
        assert(chunks[0].data_size == 64 * 1024);
        assert(!chunks[0].file_hash.empty());
        
        std::cout << "✓ Read and chunked 1MB file into " << chunks.size() << " chunks" << std::endl;
        return true;
    }
    
    // 测试2：分块序列化和反序列化
    bool test_chunk_serialization() {
        std::cout << "\n[TEST 2] Chunk serialization and deserialization..." << std::endl;
        
        FileChunk original;
        original.chunk_id = 42;
        original.total_chunks = 100;
        original.file_hash = "test_hash_12345";
        original.data = {0x01, 0x02, 0x03, 0x04, 0x05};
        original.data_size = 5;
        
        // 序列化
        auto serialized = original.serialize();
        assert(!serialized.empty());
        
        // 反序列化
        FileChunk restored;
        assert(restored.deserialize(serialized));
        
        // 验证
        assert(original.chunk_id == restored.chunk_id);
        assert(original.total_chunks == restored.total_chunks);
        assert(original.file_hash == restored.file_hash);
        assert(original.data == restored.data);
        assert(original.data_size == restored.data_size);
        
        std::cout << "✓ Serialization/deserialization successful" << std::endl;
        return true;
    }
    
    // 测试3：传输会话 - 顺序接收
    bool test_transfer_session_sequential() {
        std::cout << "\n[TEST 3] Transfer session - sequential chunk reception..." << std::endl;
        
        FileTransferSession session("hash123", test_dir + "/output1.bin", 5);
        
        // 依次添加分块
        for (int i = 0; i < 5; ++i) {
            std::vector<uint8_t> data(100 + i);
            for (size_t j = 0; j < data.size(); ++j) {
                data[j] = (i + j) % 256;
            }
            assert(session.addChunk(i, data));
        }
        
        // 验证
        assert(session.getReceivedChunkCount() == 5);
        assert(session.getProgressPercentage() == 100);
        assert(session.isComplete());
        
        std::cout << "✓ All 5 chunks received sequentially" << std::endl;
        return true;
    }
    
    // 测试4：传输会话 - 乱序接收
    bool test_transfer_session_unordered() {
        std::cout << "\n[TEST 4] Transfer session - unordered chunk reception..." << std::endl;
        
        FileTransferSession session("hash456", test_dir + "/output2.bin", 5);
        
        // 乱序添加分块
        int order[] = {2, 0, 4, 1, 3};
        for (int idx : order) {
            std::vector<uint8_t> data(100);
            session.addChunk(idx, data);
        }
        
        // 验证
        assert(session.getReceivedChunkCount() == 5);
        assert(session.getProgressPercentage() == 100);
        assert(session.isComplete());
        
        std::cout << "✓ All chunks received in random order" << std::endl;
        return true;
    }
    
    // 测试5：文件管理器 - 创建和管理会话
    bool test_file_manager_sessions() {
        std::cout << "\n[TEST 5] FileManager session management..." << std::endl;
        
        FileManager fm(test_dir + "/fm_downloads");
        
        // 创建多个会话
        fm.createTransferSession("file1", "file1.bin", 10);
        fm.createTransferSession("file2", "file2.bin", 20);
        fm.createTransferSession("file3", "file3.bin", 30);
        
        // 验证会话数
        auto sessions = fm.getActiveSessions();
        assert(sessions.size() == 3);
        
        // 添加分块到file1
        for (int i = 0; i < 5; ++i) {
            std::vector<uint8_t> data(100);
            fm.addChunkToSession("file1", i, data);
        }
        
        // 验证进度
        int progress = fm.getSessionProgress("file1");
        assert(progress == 50);  // 5/10 = 50%
        
        std::cout << "✓ Created 3 sessions, added chunks to one" << std::endl;
        return true;
    }
    
    // 测试6：完整文件传输流程
    bool test_complete_transfer() {
        std::cout << "\n[TEST 6] Complete file transfer workflow..." << std::endl;
        
        // 创建源文件
        std::string source = test_dir + "/source.bin";
        std::string content = "Hello, LAN Chat! This is a test transfer.";
        {
            std::ofstream out(source, std::ios::binary);
            out.write(content.c_str(), content.size());
        }
        
        // 发送端：读取并分块
        FileManager sender_fm(test_dir + "/sender");
        auto chunks = sender_fm.readAndChunkFile(source, 16);  // 小块便于测试
        std::string file_hash = FileManager::calculateFileHash(source);
        
        // 接收端：创建会话
        FileManager receiver_fm(test_dir + "/receiver");
        receiver_fm.createTransferSession(file_hash, "received.bin", chunks.size());
        
        // 模拟接收所有分块
        for (const auto& chunk : chunks) {
            receiver_fm.addChunkToSession(file_hash, chunk.chunk_id, chunk.data);
        }
        
        // 验证完成
        assert(receiver_fm.isTransferComplete(file_hash));
        
        // 完成传输
        assert(receiver_fm.completeTransfer(file_hash));
        
        // 验证文件内容
        std::string received_path = test_dir + "/receiver/received.bin";
        std::ifstream in(received_path, std::ios::binary);
        std::string received_content((std::istreambuf_iterator<char>(in)), 
                                     std::istreambuf_iterator<char>());
        assert(received_content == content);
        
        std::cout << "✓ Complete file transfer successful" << std::endl;
        return true;
    }
    
    // 测试7：取消传输
    bool test_cancel_transfer() {
        std::cout << "\n[TEST 7] Canceling transfer..." << std::endl;
        
        FileManager fm(test_dir + "/cancel_test");
        fm.createTransferSession("cancel_file", "test.bin", 10);
        
        // 添加几个分块
        for (int i = 0; i < 3; ++i) {
            std::vector<uint8_t> data(100);
            fm.addChunkToSession("cancel_file", i, data);
        }
        
        // 检查进度
        assert(fm.getSessionProgress("cancel_file") == 30);
        
        // 取消
        assert(fm.cancelTransfer("cancel_file"));
        
        // 验证会话已删除
        auto sessions = fm.getActiveSessions();
        assert(sessions.empty());
        
        std::cout << "✓ Transfer cancelled successfully" << std::endl;
        return true;
    }
    
    // 测试8：大文件处理
    bool test_large_file() {
        std::cout << "\n[TEST 8] Large file handling (10MB)..." << std::endl;
        
        std::string large_file = test_dir + "/large_10mb.bin";
        
        // 创建10MB文件
        {
            std::ofstream out(large_file, std::ios::binary);
            for (int i = 0; i < 10240; ++i) {
                char buffer[1024];
                for (int j = 0; j < 1024; ++j) {
                    buffer[j] = (i + j) % 256;
                }
                out.write(buffer, 1024);
            }
        }
        
        FileManager fm(test_dir + "/large_test");
        auto chunks = fm.readAndChunkFile(large_file, 256 * 1024);  // 256KB chunks
        
        // 验证
        assert(chunks.size() == 40);  // 10MB / 256KB = 40
        
        // 验证最后一块大小
        assert(chunks.back().data_size == 256 * 1024);
        
        std::cout << "✓ Large file split into " << chunks.size() << " chunks" << std::endl;
        return true;
    }
    
    bool run_all() {
        std::cout << "========================================" << std::endl;
        std::cout << "  FileManager Unit Tests" << std::endl;
        std::cout << "========================================" << std::endl;
        
        try {
            test_read_and_chunk();
            test_chunk_serialization();
            test_transfer_session_sequential();
            test_transfer_session_unordered();
            test_file_manager_sessions();
            test_complete_transfer();
            test_cancel_transfer();
            test_large_file();
            
            std::cout << "\n========================================" << std::endl;
            std::cout << "  ✓ All 8 tests passed!" << std::endl;
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

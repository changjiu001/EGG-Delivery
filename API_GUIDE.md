# 📦 文件与存储管理模块（Module 3）使用指南

## 模块概述

本模块负责 LAN Chat 应用中的文件I/O操作和传输管理，主要包括：
- ✅ 文件读写操作
- ✅ 大文件分块处理（支持任意大小文件）
- ✅ 分块缓存和管理
- ✅ 文件完整性验证
- ✅ 并发传输会话管理

## 核心类详解

### 1. `FileChunk` - 文件分块单元

**职责**：表示一个可独立传输的文件分块

#### 核心属性
```cpp
uint32_t chunk_id;           // 分块编号（0开始）
uint32_t total_chunks;       // 该文件总分块数
std::string file_hash;       // 文件唯一标识（用于校验）
std::vector<uint8_t> data;   // 实际数据内容
uint32_t data_size;          // 数据大小（字节）
```

#### 关键方法
```cpp
// 将分块转换为二进制格式，用于网络传输
std::vector<uint8_t> serialize() const;

// 从二进制数据恢复分块对象
bool deserialize(const std::vector<uint8_t>& buffer);
```

#### 使用示例
```cpp
FileChunk chunk;
chunk.chunk_id = 0;
chunk.total_chunks = 100;
chunk.file_hash = "file_abc123";
chunk.data = {/* ... */};
chunk.data_size = 65536;

// 用于发送
auto binary_data = chunk.serialize();

// 用于接收
FileChunk received_chunk;
received_chunk.deserialize(binary_data);
```

---

### 2. `FileTransferSession` - 文件传输会话

**职责**：管理单个文件的完整接收过程（分块缓存、状态跟踪、组装）

#### 构造
```cpp
FileTransferSession(
    const std::string& hash,      // 文件唯一ID
    const std::string& path,      // 保存路径
    uint32_t total                // 总分块数
);
```

#### 核心方法

```cpp
// 添加接收到的分块
bool addChunk(uint32_t chunk_id, const std::vector<uint8_t>& data);

// 检查是否所有分块都已收到
bool isComplete() const;

// 获取已接收的分块数量
uint32_t getReceivedChunkCount() const;

// 获取传输进度（0-100）
uint32_t getProgressPercentage() const;

// 将缓存的分块组装成完整文件
bool assembleFile();

// 清空内存缓存（会话结束后调用）
void clearCache();
```

#### 工作流程
```
创建会话 → 接收分块（可乱序） → 检查完成 → 组装文件 → 清理缓存
```

---

### 3. `FileManager` - 文件管理器（核心类）

**职责**：负责文件全生命周期管理和多会话调度

#### 初始化
```cpp
// 默认下载目录
FileManager fm;

// 自定义下载目录
FileManager fm("./downloads");
```

#### 发送端API

```cpp
// 读取本地文件并分块
std::vector<FileChunk> readAndChunkFile(
    const std::string& file_path,
    uint32_t chunk_size = 64*1024  // 默认64KB每块
);

// 计算文件校验哈希
static std::string calculateFileHash(const std::string& file_path);
```

**示例**：
```cpp
auto chunks = fm.readAndChunkFile("big_file.zip");

for (const auto& chunk : chunks) {
    // 通过网络发送 chunk
    send_over_network(chunk.serialize());
}
```

---

#### 接收端API

```cpp
// 1. 创建新的传输会话
bool createTransferSession(
    const std::string& file_hash,      // 文件唯一标识
    const std::string& file_name,      // 原始文件名
    uint32_t total_chunks              // 预期总分块数
);

// 2. 向会话添加接收到的分块
bool addChunkToSession(
    const std::string& file_hash,
    uint32_t chunk_id,
    const std::vector<uint8_t>& chunk_data
);

// 3. 获取传输进度
int32_t getSessionProgress(const std::string& file_hash) const;
// 返回值：0-100（百分比）或 -1（会话不存在）

// 4. 检查传输是否完成
bool isTransferComplete(const std::string& file_hash) const;

// 5. 完成传输并保存文件
bool completeTransfer(const std::string& file_hash);

// 6. 取消传输会话
bool cancelTransfer(const std::string& file_hash);
```

**完整示例**：
```cpp
FileManager fm("./downloads");

// 收到文件传输请求时
fm.createTransferSession("file_abc123", "document.pdf", 50);

// 每收到一个分块
while (receiving) {
    auto chunk = receive_from_network();
    fm.addChunkToSession("file_abc123", chunk.chunk_id, chunk.data);
    
    // 显示进度
    int progress = fm.getSessionProgress("file_abc123");
    std::cout << "Progress: " << progress << "%" << std::endl;
}

// 所有分块都接收完成后
if (fm.isTransferComplete("file_abc123")) {
    fm.completeTransfer("file_abc123");
    std::cout << "文件已保存到: " << fm.getDownloadDirectory() << std::endl;
}
```

---

#### 会话管理API

```cpp
// 获取所有活跃传输会话
std::vector<std::string> getActiveSessions() const;

// 清理空闲会话
void cleanupIdleSessions(uint32_t max_idle_time = 3600);

// 获取/设置下载目录
std::string getDownloadDirectory() const;
bool setDownloadDirectory(const std::string& dir);
```

---

## 与其他模块的集成

### 与网络通信模块（Module 1）的集成

```cpp
// 发送文件
void send_file(const std::string& file_path) {
    FileManager fm;
    auto chunks = fm.readAndChunkFile(file_path);
    
    for (const auto& chunk : chunks) {
        auto serialized = chunk.serialize();
        socket_send(serialized);  // 调用网络模块的发送函数
    }
}

// 接收文件
void receive_file_handler(const std::string& file_hash, uint32_t total_chunks) {
    FileManager fm;
    fm.createTransferSession(file_hash, "received_file", total_chunks);
    
    // 在网络数据到达时调用
    void on_chunk_received(const std::vector<uint8_t>& data) {
        FileChunk chunk;
        chunk.deserialize(data);
        fm.addChunkToSession(file_hash, chunk.chunk_id, chunk.data);
    }
}
```

### 与数据协议模块（Module 2）的集成

```cpp
// 传输开始通知
struct FileTransferStart {
    std::string file_hash;
    std::string file_name;
    uint32_t total_chunks;
    uint64_t total_size;
};

// 分块数据包
struct ChunkPacket {
    std::string file_hash;
    FileChunk chunk;
};

// 传输完成确认
struct FileTransferComplete {
    std::string file_hash;
    bool success;
};
```

### 与UI模块（Module 4）的集成

```cpp
// UI可以通过这些接口展示传输状态
class FileTransferUI {
    void show_transfer_progress(const std::string& file_hash) {
        FileManager fm;
        int progress = fm.getSessionProgress(file_hash);
        if (progress >= 0) {
            display_progress_bar(progress);
        }
    }
    
    void list_active_transfers() {
        FileManager fm;
        auto sessions = fm.getActiveSessions();
        for (const auto& hash : sessions) {
            std::cout << hash << ": " 
                      << fm.getSessionProgress(hash) << "%" << std::endl;
        }
    }
};
```

---

## 性能特性

| 特性 | 说明 |
|------|------|
| 默认分块大小 | 64 KB |
| 支持最大文件 | 无限制（仅受系统内存限制） |
| 并发会话 | 理论无限（实际受内存限制） |
| 分块乱序处理 | 完全支持 |
| 缓存机制 | 所有分块缓存在内存中 |

---

## 错误处理

```cpp
// 建议的错误处理模式
bool transfer_file_safely(FileManager& fm, const std::string& file_path) {
    // 1. 读取文件
    auto chunks = fm.readAndChunkFile(file_path);
    if (chunks.empty()) {
        std::cerr << "Failed to read file" << std::endl;
        return false;
    }
    
    // 2. 创建会话
    std::string file_hash = FileManager::calculateFileHash(file_path);
    if (!fm.createTransferSession(file_hash, "test.file", chunks.size())) {
        std::cerr << "Failed to create session" << std::endl;
        return false;
    }
    
    // 3. 发送分块（带重试）
    for (const auto& chunk : chunks) {
        if (!send_chunk_with_retry(chunk)) {
            fm.cancelTransfer(file_hash);
            return false;
        }
    }
    
    // 4. 完成传输
    if (!fm.completeTransfer(file_hash)) {
        std::cerr << "Failed to finalize transfer" << std::endl;
        return false;
    }
    
    return true;
}
```

---

## 编译和链接

### CMakeLists.txt 示例
```cmake
add_library(file_manager
    file_manager.cpp
    file_manager.h
)

target_include_directories(file_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(file_manager PRIVATE stdc++fs)  # C++17 filesystem
```

### 编译命令
```bash
# 编译示例程序
g++ -std=c++17 -o example example.cpp file_manager.cpp -lstdc++fs

# 运行
./example
```

---

## 常见问题

**Q: 如果分块在传输中丢失怎么办？**
A: 应该在协议层（Module 2）实现重传机制。文件管理器负责接收端的缓存和组装。

**Q: 分块大小应该设置多大？**
A: 建议 64KB-256KB。太小会增加分块数和开销，太大容易导致单个包丢失影响范围大。

**Q: 支持断点续传吗？**
A: 当前版本不支持。可以在未来版本添加持久化会话状态到磁盘。

**Q: 内存占用会很大吗？**
A: 所有分块数据都缓存在内存。100MB 文件分成 64KB 块 = 1600+ 个 vector，需要约 100MB 内存。

---

## 接下来的优化方向

- [ ] 添加真正的 MD5/SHA256 哈希计算
- [ ] 实现断点续传功能
- [ ] 添加分块校验和验证
- [ ] 性能优化（使用内存池）
- [ ] 单元测试覆盖
- [ ] 多线程接收支持


# 🔗 Module 3 与其他模块的集成指南

## 总体架构

```
┌──────────────────────────────────────────────────────────────┐
│                   UI & 交互层 (Module 4)                      │
│  - 文件选择对话框                                              │
│  - 传输进度显示                                                │
│  - 文件列表 UI                                                │
└─────────────────────┬──────────────────────────────────────────┘
                      │
                      ↓
┌──────────────────────────────────────────────────────────────┐
│            📦 文件与存储管理 (Module 3)                       │
│  - FileManager     (会话管理)                                 │
│  - FileChunk       (分块单元)                                │
│  - FileTransferSession (传输状态)                             │
└─────────────────────┬──────────────────────────────────────────┘
                      │
                      ↓
┌──────────────────────────────────────────────────────────────┐
│          数据与协议工程师 (Module 2)                          │
│  - 消息序列化/反序列化                                        │
│  - 协议消息定义                                               │
│  - 分块数据包格式                                             │
└─────────────────────┬──────────────────────────────────────────┘
                      │
                      ↓
┌──────────────────────────────────────────────────────────────┐
│           网络通信架构师 (Module 1)                           │
│  - Socket 编程                                               │
│  - TCP/UDP 数据传输                                          │
│  - 连接管理                                                  │
└──────────────────────────────────────────────────────────────┘
```

---

## 集成步骤

### 步骤 1：集成 Module 3 到项目

#### 1.1 复制文件
```bash
# 从 Module 3 的目录
cp file_manager.h /path/to/your/project/modules/file_manager/
cp file_manager.cpp /path/to/your/project/modules/file_manager/
```

#### 1.2 更新 CMakeLists.txt
```cmake
# 在主项目的 CMakeLists.txt 中
add_subdirectory(modules/file_manager)

# 链接到其他模块
target_link_libraries(your_main_executable PRIVATE file_manager)
```

---

### 步骤 2：定义 Module 2 的消息协议

Module 2 需要定义以下消息类型：

```cpp
// protocol.h
#pragma once
#include "file_manager.h"
#include <cstdint>
#include <string>
#include <vector>

// 消息类型枚举
enum class MessageType : uint8_t {
    CHAT_TEXT = 0x01,
    FILE_TRANSFER_START = 0x10,
    FILE_TRANSFER_CHUNK = 0x11,
    FILE_TRANSFER_COMPLETE = 0x12,
    FILE_TRANSFER_CANCEL = 0x13,
};

// ============= 文件传输消息 =============

// 文件传输开始请求
struct FileTransferStart {
    std::string file_hash;        // 文件唯一ID
    std::string file_name;        // 原始文件名
    uint64_t file_size;           // 文件总大小
    uint32_t total_chunks;        // 总分块数
    uint32_t chunk_size;          // 每块大小
    
    // 序列化
    std::vector<uint8_t> serialize() const;
    
    // 反序列化
    bool deserialize(const std::vector<uint8_t>& data);
};

// 文件分块数据包
struct FileTransferChunk {
    std::string file_hash;           // 文件ID
    FileChunk chunk;                 // 分块数据（来自 Module 3）
    
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
};

// 文件传输完成确认
struct FileTransferComplete {
    std::string file_hash;
    bool success;
    std::string message;
    
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
};

// 文件传输取消
struct FileTransferCancel {
    std::string file_hash;
    std::string reason;
    
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
};
```

---

### 步骤 3：Module 1（网络层）的使用

Module 1 需要提供这些接口：

```cpp
// network.h
class NetworkManager {
public:
    // 发送原始字节数据
    bool send(const std::string& peer_id, const std::vector<uint8_t>& data);
    
    // 接收原始字节数据（回调）
    std::function<void(const std::string& peer_id, const std::vector<uint8_t>& data)> 
        on_data_received;
};
```

---

### 步骤 4：完整的文件传输流程实现

#### 4.1 发送端流程（Sender）

```cpp
class FileSender {
private:
    FileManager file_manager;
    NetworkManager& network;
    ProtocolEncoder& protocol;
    
public:
    FileSender(NetworkManager& net, ProtocolEncoder& proto) 
        : file_manager("./downloads"), network(net), protocol(proto) {}
    
    // 发送文件
    bool sendFile(const std::string& peer_id, const std::string& file_path) {
        // 1. 读取并分块
        auto chunks = file_manager.readAndChunkFile(file_path);
        if (chunks.empty()) {
            std::cerr << "Failed to read file" << std::endl;
            return false;
        }
        
        // 2. 准备文件传输开始消息
        FileTransferStart start_msg;
        start_msg.file_hash = FileManager::calculateFileHash(file_path);
        start_msg.file_name = fs::path(file_path).filename().string();
        start_msg.file_size = fs::file_size(file_path);
        start_msg.total_chunks = chunks.size();
        start_msg.chunk_size = chunks[0].data_size;
        
        // 3. 发送文件传输开始消息
        auto start_data = start_msg.serialize();
        if (!network.send(peer_id, start_data)) {
            std::cerr << "Failed to send start message" << std::endl;
            return false;
        }
        
        std::cout << "File transfer start sent: " << start_msg.file_name << std::endl;
        
        // 4. 发送所有分块
        for (const auto& chunk : chunks) {
            FileTransferChunk chunk_msg;
            chunk_msg.file_hash = start_msg.file_hash;
            chunk_msg.chunk = chunk;
            
            auto chunk_data = chunk_msg.serialize();
            if (!network.send(peer_id, chunk_data)) {
                std::cerr << "Failed to send chunk " << chunk.chunk_id << std::endl;
                return false;
            }
            
            std::cout << "Sent chunk " << chunk.chunk_id << "/" 
                      << chunk.total_chunks << std::endl;
        }
        
        // 5. 发送完成消息
        FileTransferComplete complete_msg;
        complete_msg.file_hash = start_msg.file_hash;
        complete_msg.success = true;
        complete_msg.message = "Transfer completed";
        
        auto complete_data = complete_msg.serialize();
        network.send(peer_id, complete_data);
        
        std::cout << "File transfer completed" << std::endl;
        return true;
    }
};
```

#### 4.2 接收端流程（Receiver）

```cpp
class FileReceiver {
private:
    FileManager file_manager;
    NetworkManager& network;
    ProtocolDecoder& protocol;
    std::map<std::string, TransferState> active_transfers;
    
    struct TransferState {
        FileTransferStart info;
        std::chrono::system_clock::time_point start_time;
    };
    
public:
    FileReceiver(NetworkManager& net, ProtocolDecoder& proto) 
        : file_manager("./downloads"), network(net), protocol(proto) {
        // 注册数据接收回调
        network.on_data_received = [this](const std::string& peer_id, 
                                          const std::vector<uint8_t>& data) {
            this->handleIncomingData(peer_id, data);
        };
    }
    
    // 处理接收到的数据
    void handleIncomingData(const std::string& peer_id, 
                           const std::vector<uint8_t>& data) {
        // 1. 解析消息类型
        if (data.size() < 1) return;
        
        MessageType msg_type = static_cast<MessageType>(data[0]);
        
        switch (msg_type) {
            case MessageType::FILE_TRANSFER_START:
                handleTransferStart(peer_id, data);
                break;
            case MessageType::FILE_TRANSFER_CHUNK:
                handleTransferChunk(peer_id, data);
                break;
            case MessageType::FILE_TRANSFER_COMPLETE:
                handleTransferComplete(peer_id, data);
                break;
            case MessageType::FILE_TRANSFER_CANCEL:
                handleTransferCancel(peer_id, data);
                break;
            default:
                break;
        }
    }
    
private:
    void handleTransferStart(const std::string& peer_id, 
                            const std::vector<uint8_t>& data) {
        // 1. 解析开始消息
        FileTransferStart start_msg;
        if (!start_msg.deserialize(data)) {
            std::cerr << "Failed to parse transfer start message" << std::endl;
            return;
        }
        
        // 2. 创建文件管理器会话
        if (!file_manager.createTransferSession(
            start_msg.file_hash, 
            start_msg.file_name, 
            start_msg.total_chunks)) {
            std::cerr << "Failed to create transfer session" << std::endl;
            return;
        }
        
        // 3. 记录传输状态
        TransferState state;
        state.info = start_msg;
        state.start_time = std::chrono::system_clock::now();
        active_transfers[start_msg.file_hash] = state;
        
        std::cout << "File transfer started: " << start_msg.file_name 
                  << " (" << start_msg.total_chunks << " chunks)" << std::endl;
    }
    
    void handleTransferChunk(const std::string& peer_id, 
                            const std::vector<uint8_t>& data) {
        // 1. 解析分块消息
        FileTransferChunk chunk_msg;
        if (!chunk_msg.deserialize(data)) {
            std::cerr << "Failed to parse transfer chunk" << std::endl;
            return;
        }
        
        // 2. 添加到文件管理器
        if (!file_manager.addChunkToSession(
            chunk_msg.file_hash, 
            chunk_msg.chunk.chunk_id, 
            chunk_msg.chunk.data)) {
            std::cerr << "Failed to add chunk to session" << std::endl;
            return;
        }
        
        // 3. 显示进度
        int progress = file_manager.getSessionProgress(chunk_msg.file_hash);
        std::cout << "Transfer progress: " << progress << "%" << std::endl;
    }
    
    void handleTransferComplete(const std::string& peer_id, 
                               const std::vector<uint8_t>& data) {
        // 1. 解析完成消息
        FileTransferComplete complete_msg;
        if (!complete_msg.deserialize(data)) {
            std::cerr << "Failed to parse transfer complete" << std::endl;
            return;
        }
        
        if (!complete_msg.success) {
            std::cerr << "Transfer failed: " << complete_msg.message << std::endl;
            file_manager.cancelTransfer(complete_msg.file_hash);
            return;
        }
        
        // 2. 完成传输
        if (!file_manager.completeTransfer(complete_msg.file_hash)) {
            std::cerr << "Failed to complete transfer" << std::endl;
            return;
        }
        
        // 3. 清理记录
        active_transfers.erase(complete_msg.file_hash);
        
        // 4. 计算传输统计
        auto it = active_transfers.find(complete_msg.file_hash);
        if (it != active_transfers.end()) {
            auto duration = std::chrono::system_clock::now() - it->second.start_time;
            std::cout << "File transfer completed in " 
                      << std::chrono::duration_cast<std::chrono::seconds>(duration).count() 
                      << " seconds" << std::endl;
        }
    }
    
    void handleTransferCancel(const std::string& peer_id, 
                             const std::vector<uint8_t>& data) {
        // 1. 解析取消消息
        FileTransferCancel cancel_msg;
        if (!cancel_msg.deserialize(data)) {
            std::cerr << "Failed to parse transfer cancel" << std::endl;
            return;
        }
        
        // 2. 取消传输
        file_manager.cancelTransfer(cancel_msg.file_hash);
        active_transfers.erase(cancel_msg.file_hash);
        
        std::cout << "Transfer cancelled: " << cancel_msg.reason << std::endl;
    }
};
```

---

### 步骤 5：UI 层集成（Module 4）

```cpp
// ui_file_transfer.h
class FileTransferUI {
private:
    FileManager& file_manager;
    
public:
    FileTransferUI(FileManager& fm) : file_manager(fm) {}
    
    // 显示文件选择对话框并开始发送
    void selectAndSendFile(const std::string& peer_id) {
        // 打开文件选择对话框（使用平台特定 API）
        std::string selected_file = showFileDialog();
        if (selected_file.empty()) return;
        
        // 通过 Module 2 的发送接口发送
        FileSender sender(network, protocol);
        sender.sendFile(peer_id, selected_file);
    }
    
    // 显示传输进度
    void showTransferProgress() {
        auto sessions = file_manager.getActiveSessions();
        
        for (const auto& hash : sessions) {
            int progress = file_manager.getSessionProgress(hash);
            drawProgressBar(hash, progress);
        }
    }
    
    // 取消传输
    void cancelTransfer(const std::string& file_hash) {
        file_manager.cancelTransfer(file_hash);
    }
    
private:
    std::string showFileDialog() {
        // 实现文件选择对话框
        // 可以使用 Raylib 或其他 UI 库
        return "";
    }
    
    void drawProgressBar(const std::string& hash, int progress) {
        // 绘制进度条
    }
};
```

---

## 编译集成项目

### 完整 CMakeLists.txt 示例

```cmake
cmake_minimum_required(VERSION 3.10)
project(EGG_Delivery_LAN_Chat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== Module 3: File Manager ==========
add_library(file_manager STATIC
    modules/file_manager/file_manager.cpp
    modules/file_manager/file_manager.h
)
target_include_directories(file_manager PUBLIC modules/file_manager)
target_link_libraries(file_manager PRIVATE stdc++fs)

# ========== Module 2: Protocol ==========
add_library(protocol STATIC
    modules/protocol/protocol.cpp
    modules/protocol/protocol.h
)
target_include_directories(protocol PUBLIC modules/protocol)
target_link_libraries(protocol PRIVATE file_manager)

# ========== Module 1: Network ==========
add_library(network STATIC
    modules/network/network.cpp
    modules/network/network.h
)
target_include_directories(network PUBLIC modules/network)

# ========== Module 4: UI ==========
add_library(ui STATIC
    modules/ui/ui.cpp
    modules/ui/ui.h
)
target_include_directories(ui PUBLIC modules/ui)
target_link_libraries(ui PRIVATE file_manager protocol network)

# ========== Main Application ==========
add_executable(lan_chat_app
    main.cpp
)
target_link_libraries(lan_chat_app PRIVATE 
    file_manager protocol network ui
)
```

---

## 测试集成

```cpp
// test_integration.cpp
#include "file_manager.h"
#include "protocol.h"
#include "network.h"
#include <iostream>

int main() {
    std::cout << "Testing Module 3 integration...\n\n";
    
    // 测试 FileManager
    FileManager fm("./downloads");
    
    // 测试 Protocol
    FileTransferStart start_msg;
    start_msg.file_hash = "test_hash";
    start_msg.file_name = "test.bin";
    start_msg.total_chunks = 10;
    
    auto serialized = start_msg.serialize();
    std::cout << "Protocol serialization: " << serialized.size() << " bytes\n";
    
    // 测试 Network + Protocol + FileManager
    FileReceiver receiver(network, protocol);
    receiver.handleIncomingData("peer1", serialized);
    
    std::cout << "✓ Integration test passed!\n";
    return 0;
}
```

---

## 常见问题

**Q: Module 3 如何与 Module 1 通信？**
A: 通过 Module 2 的 Protocol 层作为中介。Module 1 提供原始套接字，Module 2 提供高层消息格式，Module 3 处理文件逻辑。

**Q: 如何处理传输失败？**
A: 在 Module 2 的协议中添加重试机制，在 Module 3 中实现文件校验。

**Q: 内存占用如何优化？**
A: 可以修改 Module 3 使用流式读写而不是缓存所有分块。


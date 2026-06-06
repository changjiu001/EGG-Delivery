# EGG-Delivery 📦 Module 3 - 文件与存储管理

SZTU 面向对象开发小组项目 | LAN Chat 应用的文件存储管理模块

## 🎯 模块概述

这是 **EGG-Delivery（LAN Chat）项目的第三个模块**，负责处理所有文件的 I/O 操作和网络传输。

### 模块职责
- ✅ **文件读写管理** - 使用 C++ 的文件流进行高效 I/O 操作
- ✅ **大文件分块** - 将任意大小的文件分成可独立传输的分块
- ✅ **传输会话管理** - 管理多个并发文件接收会话
- ✅ **数据完整性验证** - 通过哈希值验证文件的一致性
- ✅ **面向对象设计** - 充分体现继承、封装、多态的设计思想

### 技术栈
- **语言**: C++17
- **构建系统**: CMake
- **依赖**: C++ 标准库 (filesystem)
- **编译器**: GCC/Clang/MSVC

---

## 📁 项目结构

```
.
├── file_manager.h          # 核心类定义
├── file_manager.cpp        # 核心实现
├── example.cpp             # 使用示例（4个完整演示）
├── test.cpp                # 单元测试（8个测试用例）
├── API_GUIDE.md            # 详细 API 文档
├── CMakeLists.txt          # 构建配置
└── README.md               # 本文件
```

---

## 🚀 快速开始

### 编译

```bash
# 方式1: 使用 CMake
mkdir build && cd build
cmake ..
cmake --build .

# 方式2: 直接编译（Linux/Mac）
g++ -std=c++17 -o example example.cpp file_manager.cpp -lstdc++fs
g++ -std=c++17 -o test test.cpp file_manager.cpp -lstdc++fs
```

### 运行示例

```bash
./example
```

输出包括：
1. 📖 文件读取与分块演示
2. 📥 文件接收与组装演示
3. 🔄 并发传输演示
4. 🔐 分块序列化演示

### 运行测试

```bash
./test
# 或通过 CMake
ctest --verbose
```

---

## 📚 核心类详解

### 1️⃣ `FileChunk` - 分块单元

表示一个可独立传输的文件分块

```cpp
FileChunk chunk;
chunk.chunk_id = 0;              // 分块编号
chunk.total_chunks = 100;        // 总分块数
chunk.file_hash = "abc123";      // 文件唯一ID
chunk.data = {/* ... */};        // 实际数据
chunk.data_size = 65536;         // 数据大小

// 网络传输前序列化
auto binary = chunk.serialize();

// 网络接收后反序列化
FileChunk received;
received.deserialize(binary);
```

### 2️⃣ `FileTransferSession` - 传输会话

管理单个文件的完整接收过程

```cpp
FileTransferSession session("file_hash", "./save_path.bin", 50);

// 接收分块（支持乱序）
session.addChunk(0, data);
session.addChunk(5, data);
session.addChunk(3, data);
// ...

// 查询进度
int progress = session.getProgressPercentage();  // 0-100%

// 完成后组装
if (session.isComplete()) {
    session.assembleFile();  // 写入磁盘
}
```

### 3️⃣ `FileManager` - 管理器（核心）

管理文件的全生命周期和多会话调度

```cpp
FileManager fm("./downloads");

// === 发送端 ===
auto chunks = fm.readAndChunkFile("large_file.zip", 64*1024);
for (const auto& chunk : chunks) {
    send_over_network(chunk);
}

// === 接收端 ===
fm.createTransferSession("file_hash", "received.zip", num_chunks);

// 每收到一个分块
fm.addChunkToSession("file_hash", chunk_id, chunk_data);

// 显示进度
int progress = fm.getSessionProgress("file_hash");

// 完成
if (fm.isTransferComplete("file_hash")) {
    fm.completeTransfer("file_hash");
}
```

---

## 💡 使用场景

### 场景1: 发送小文件
```cpp
FileManager fm;
auto chunks = fm.readAndChunkFile("message.txt");
for (auto& chunk : chunks) {
    network.send(chunk.serialize());
}
```

### 场景2: 接收大文件
```cpp
FileManager fm;
fm.createTransferSession(file_hash, filename, total_chunks);

// 在网络回调中
while (receiving) {
    auto chunk = network.receive();
    fm.addChunkToSession(file_hash, chunk.chunk_id, chunk.data);
    update_progress_bar(fm.getSessionProgress(file_hash));
}

fm.completeTransfer(file_hash);
```

### 场景3: 多文件并发传输
```cpp
FileManager fm;

// 同时接收3个文件
fm.createTransferSession("file1_hash", "file1.bin", 100);
fm.createTransferSession("file2_hash", "file2.zip", 200);
fm.createTransferSession("file3_hash", "file3.doc", 50);

// 查看所有活跃传输
auto sessions = fm.getActiveSessions();
for (auto& hash : sessions) {
    std::cout << hash << ": " << fm.getSessionProgress(hash) << "%" << std::endl;
}
```

---

## 🔗 与其他模块的集成

```
┌─────────────────────────────────────────────┐
│  Module 4: UI & 交互设计师                  │
│  ├─ 显示传输进度条                           │
│  └─ 管理文件列表 UI                          │
└────────────────┬────────────────────────────┘
                 │ 调用
┌────────────────▼────────────────────────────┐
│  Module 3: 文件与存储管理（本模块）         │
│  ├─ 文件读写 I/O                             │
│  ├─ 分块处理                                │
│  └─ 会话管理                                │
└────────────────┬────────────────────────────┘
                 │ 提供数据
┌────────────────▼────────────────────────────┐
│  Module 2: 数据与协议工程师                 │
│  ├─ 序列化/反序列化                         │
│  ├─ 消息打包格式                            │
│  └─ 局域网发现协议                          │
└────────────────┬────────────────────────────┘
                 │ 调用
┌────────────────▼────────────────────────────┐
│  Module 1: 网络通信架构师（Socket）         │
│  ├─ TCP/UDP Socket 编程                    │
│  └─ 数据传输                                │
└─────────────────────────────────────────────┘
```

### 与 Module 1（网络层）集成
```cpp
// 发送文件
auto chunks = fm.readAndChunkFile(file_path);
for (auto& chunk : chunks) {
    socket->send(chunk.serialize());
}

// 接收文件
void on_data_received(const std::vector<uint8_t>& data) {
    FileChunk chunk;
    chunk.deserialize(data);
    fm.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data);
}
```

### 与 Module 2（协议层）集成
```cpp
// 定义消息包装
struct FileTransferMessage {
    std::string type;  // "START", "CHUNK", "COMPLETE"
    FileChunk chunk;
};
```

### 与 Module 4（UI层）集成
```cpp
// UI 查询进度
int progress = fm.getSessionProgress(file_hash);
ui->setProgressBar(progress);

// UI 列出所有传输
auto sessions = fm.getActiveSessions();
ui->updateTransferList(sessions);
```

---

## 📊 性能指标

| 指标 | 说明 |
|------|------|
| 默认分块大小 | 64 KB |
| 支持文件大小 | 无限制（仅受系统内存限制） |
| 最大并发传输 | 理论无限（实际受内存限制） |
| 分块乱序处理 | ✅ 完全支持 |
| 缓存机制 | 所有分块在内存中缓存 |

### 内存使用估算
- 100MB 文件 + 64KB 分块 = ~1600 块 = 约 100MB 内存
- 1GB 文件 + 64KB 分块 = ~16000 块 = 约 1GB 内存

---

## ✅ 测试用例

```
[TEST 1] 文件读取和分块 ✓
[TEST 2] 分块序列化/反序列化 ✓
[TEST 3] 传输会话 - 顺序接收 ✓
[TEST 4] 传输会话 - 乱序接收 ✓
[TEST 5] 文件管理器会话管理 ✓
[TEST 6] 完整文件传输工作流 ✓
[TEST 7] 取消传输 ✓
[TEST 8] 大文件处理（10MB） ✓
```

运行所有测试：
```bash
./test
```

---

## 🛠️ API 文档

完整的 API 文档请参考 [API_GUIDE.md](./API_GUIDE.md)

### 关键方法速查

**发送端**
```cpp
FileManager fm;
fm.readAndChunkFile(file_path, chunk_size);
FileManager::calculateFileHash(file_path);
```

**接收端**
```cpp
fm.createTransferSession(hash, filename, total_chunks);
fm.addChunkToSession(hash, chunk_id, data);
fm.getSessionProgress(hash);           // 返回 0-100
fm.isTransferComplete(hash);
fm.completeTransfer(hash);
fm.cancelTransfer(hash);
fm.getActiveSessions();
```

---

## 🎓 学习价值

通过本模块的实现，可以学到：

- ✅ **C++ 文件 I/O** - ifstream/ofstream 的使用
- ✅ **内存管理** - vector 容器和缓存策略
- ✅ **面向对象设计** - 类设计、封装、多态
- ✅ **二进制序列化** - 数据格式化和传输
- ✅ **错误处理** - 异常处理和验证
- ✅ **算法优化** - 分块策略和哈希计算

---

## 🔮 未来优化方向

- [ ] 真正的 MD5/SHA256 哈希计算（当前为简化版）
- [ ] 断点续传功能
- [ ] 分块校验和验证
- [ ] 内存池优化（减少频繁分配）
- [ ] 多线程并发接收
- [ ] 持久化会话状态

---

## 📝 许可证

本项目是 SZTU 面向对象开发小组项目的一部分。

---

## 👥 模块作者

- **Module 3 负责人**: [你的名字]
- **项目协调**: SZTU 面向对象开发小组

---

## 📞 问题反馈

如有任何问题或建议，请提交 Issue 或 PR。

参考文档：[API 完整指南](./API_GUIDE.md) | [使用示例](./example.cpp) | [单元测试](./test.cpp)

# EGG-Delivery

无中心服务器的局域网聊天 + 文件传输 | C++17 · Socket

---

## 模块

| 模块 | 负责人 | 状态 |
|---|---|---|
| 1. 网络通信（Socket） | 待定 | 待开发 |
| 2. 数据与协议 | 待定 | 待开发 |
| 3. 文件与存储管理 | refrain321 | 已完成 |
| 4. UI & 交互 | 待定 | 待开发 |

---

## 结构

```
├── CMakeLists.txt
├── file_manager.h / .cpp    # Module 3
├── example.cpp
├── test.cpp
├── API_GUIDE.md
├── INTEGRATION.md
└── modules/                 # 其他模块放这里
    ├── network/
    ├── protocol/
    └── ui/
```

---

## 构建

```bash
g++ -std=c++17 -o example example.cpp file_manager.cpp
g++ -std=c++17 -o test test.cpp file_manager.cpp

# 或 CMake
mkdir build && cd build && cmake .. && cmake --build .
```

---

## Module 3 接口

`#include "file_manager.h"` 即可。详见 [API_GUIDE.md](./API_GUIDE.md)、[INTEGRATION.md](./INTEGRATION.md)。

### 发送

```cpp
FileManager fm("./downloads");
auto chunks = fm.readAndChunkFile("file.bin");  // 默认 64KB/块
for (auto& c : chunks) {
    auto wire = c.serialize();   // → vector<uint8_t>，交给 Socket 发
}
```

### 接收

```cpp
FileManager fm("./downloads");
fm.createTransferSession(file_hash, "save_as.bin", total_chunks);

// Socket 每收到一包：
FileChunk chunk;
if (chunk.deserialize(wire_bytes)) {          // 自动 CRC 校验
    fm.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data);
}

if (fm.isTransferComplete(file_hash)) {
    fm.completeTransfer(file_hash);           // 写入磁盘
}
```

### 进度 & 管理

```cpp
int  pct  = fm.getSessionProgress(hash);       // 0-100, -1=不存在
auto list = fm.getActiveSessions();            // 所有活跃传输
fm.cancelTransfer(hash);                       // 取消
fm.cleanupIdleSessions(3600);                  // 清理空闲 >1h 的会话
```

---

## 协议（v1）

```
[4B magic 0x4C414E43] [1B version=1] [4B chunk_id] [4B total_chunks]
[4B data_size] [4B CRC32] [4B hash_len] [64B SHA256 hex] [data]
```

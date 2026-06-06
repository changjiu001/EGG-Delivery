# Module 3 — 文件与存储管理 API 指南

## 快速索引

| 你要做什么 | 用什么 |
|---|---|
| 把文件切成块准备发送 | `FileManager::readAndChunkFile()` |
| 把块序列化成网络字节流 | `FileChunk::serialize()` |
| 从网络字节流恢复分块（含 CRC 校验） | `FileChunk::deserialize()` |
| 创建接收会话 | `FileManager::createTransferSession()` |
| 收到一块数据，存进去 | `FileManager::addChunkToSession()` |
| 查进度（给 UI 用） | `FileManager::getSessionProgress()` |
| 传完了，写入磁盘 | `FileManager::completeTransfer()` |
| 算文件 SHA256 | `SHA256::hashFile()` |
| 算数据 CRC32 | `crc32()` |

---

## 核心类

### `SHA256` — 文件/数据哈希

```cpp
// 文件哈希
std::string hash = SHA256::hashFile("path/to/file.bin");
// → 64 字符十六进制小写，如 "ba7816bf8f01cfea..."

// 内存数据哈希
std::string hash = SHA256::hashData(data_ptr, data_len);

// 增量哈希（流式）
SHA256 sha;
sha.update(chunk1).update(chunk2);
std::string result = sha.digest();
```

### `crc32()` — 数据校验

```cpp
uint32_t c = crc32(data.data(), data.size());
// → 标准 CRC-32（与 Ethernet / gzip 一致）
```

### `FileChunk` — 分块单元

```cpp
FileChunk chunk;
chunk.chunk_id     = 0;          // 第几块（从 0 开始）
chunk.total_chunks = 100;        // 总共多少块
chunk.file_hash    = "...";      // 64 字符 SHA256
chunk.data         = {...};      // 实际数据
chunk.data_size    = 65536;      // 数据字节数

// 序列化（含协议头 + CRC32）
auto wire = chunk.serialize();

// 反序列化（自动校验魔数 + 版本 + CRC32）
FileChunk received;
bool ok = received.deserialize(wire);  // false = 损坏或非本协议
```

### `FileTransferSession` — 传输会话

```cpp
// 通常不直接创建，而是通过 FileManager
FileTransferSession session(hash, save_path, total_chunks);

session.addChunk(id, data);           // 添加分块（支持乱序）
session.isComplete();                 // 是否全部到齐
session.getProgressPercentage();      // 0–100
session.getReceivedChunkCount();      // 已收到几块
session.getMissingChunks();           // → vector<uint32_t> 还没到的块号
session.assembleFile();               // 按序写入磁盘 + SHA256 校验
session.secondsIdle();                // 距上次 addChunk 过去多少秒
```

### `FileManager` — 总入口

```cpp
FileManager fm("./downloads");        // 下载目录，自动创建

// —— 发送 ——
auto chunks = fm.readAndChunkFile("file.zip");          // 默认 64KB/块
auto chunks = fm.readAndChunkFile("file.iso", 256*1024); // 自定义块大小

// —— 接收 ——
fm.createTransferSession(file_hash, "filename.ext", total_chunks);
fm.addChunkToSession(file_hash, chunk_id, data);
fm.getSessionProgress(file_hash);     // 0–100，不存在返回 -1
fm.isTransferComplete(file_hash);     // true/false
fm.completeTransfer(file_hash);       // 写入磁盘 + 清除会话
fm.cancelTransfer(file_hash);         // 丢弃 + 清除会话

// —— 管理 ——
fm.getActiveSessions();               // → vector<string> 所有活跃会话 hash
fm.sessionCount();                    // 活跃会话数
fm.cleanupIdleSessions(3600);         // 清理空闲 > 1 小时的会话
fm.setDownloadDirectory("/new/path");
fm.getDownloadDirectory();
```

---

## 协议格式（v1）

```
Byte  0-3:   Magic  "LANC" (0x4C414E43)
Byte  4:     Version (1)
Byte  5-8:   chunk_id      (uint32 BE)
Byte  9-12:  total_chunks  (uint32 BE)
Byte 13-16:  data_size     (uint32 BE)
Byte 17-20:  CRC32 of data (uint32 BE)
Byte 21-24:  hash_len      (uint32 BE)
Byte 25..:   hash (SHA256, 64 ASCII hex chars)
Then:        data (data_size bytes)
```

---

## 完整示例

```cpp
#include "file_manager.h"

int main() {
    FileManager fm("./downloads");

    // === 发送端 ===
    auto chunks = fm.readAndChunkFile("report.pdf");
    for (const auto& c : chunks) {
        auto wire = c.serialize();       // 序列化
        send_over_network(wire);         // 交给 Module 1
    }

    // === 接收端 ===
    std::string hash = chunks[0].file_hash;
    fm.createTransferSession(hash, "report.pdf", chunks.size());

    // ...每收到一包数据...
    FileChunk arrived;
    if (arrived.deserialize(received_bytes)) {
        fm.addChunkToSession(hash, arrived.chunk_id, arrived.data);
    }

    if (fm.isTransferComplete(hash)) {
        fm.completeTransfer(hash);       // → ./downloads/report.pdf
    }
}
```

---

## 测试

```bash
g++ -std=c++17 -o test test.cpp file_manager.cpp && ./test
# ✓ All 12 tests passed!
```

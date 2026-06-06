# Module 3 集成指引

> **如果你是 Module 1 / 2 / 4 的开发者，看这篇就够了。**  
> 只需要 `#include "file_manager.h"`，其他什么都不用装。

---

## 给 Module 1（网络通信）开发者

你的职责：在局域网两台电脑之间搬运字节流。Module 3 把文件变成了 `vector<uint8_t>`，你只管发。

### 发送文件

```cpp
#include "file_manager.h"

// 1. 创建文件管理器
FileManager fm("./downloads");

// 2. 把文件切成块
auto chunks = fm.readAndChunkFile("要发的文件.zip");
//    ↑ 默认每块 64KB，也可以指定大小：
//    auto chunks = fm.readAndChunkFile("大文件.iso", 256 * 1024);

// 3. 逐块序列化 → 交给你发送
for (const auto& chunk : chunks) {
    std::vector<uint8_t> wire_bytes = chunk.serialize();
    your_socket_send(wire_bytes);   // ← 你的发送函数
}
```

### 接收文件

```cpp
#include "file_manager.h"

FileManager fm("./downloads");

// 这个函数在你的网络回调里被调用
void on_bytes_arrived(const std::vector<uint8_t>& wire_bytes) {
    FileChunk chunk;

    // 反序列化 + 自动 CRC32 校验
    if (!chunk.deserialize(wire_bytes)) {
        // 数据损坏或不是我们的协议 → 丢弃
        return;
    }

    // 交给 FileManager 缓存
    fm.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data);
}
```

### 你不需要关心的

- 分块大小怎么选（默认 64KB 够用）
- CRC32 怎么算（序列化/反序列化自动处理）
- 乱序到达怎么办（FileManager 内部排序）

---

## 给 Module 2（协议）开发者

你的职责：定义消息外层——这条消息是文字还是文件分块、谁发的。Module 3 提供**文件载荷的序列化格式**。

### 文件传输消息定义（建议）

```cpp
#include "file_manager.h"
#include <cstdint>

enum class MessageType : uint8_t {
    CHAT_TEXT            = 0x01,
    FILE_TRANSFER_START  = 0x10,   // 开始传输文件
    FILE_TRANSFER_CHUNK  = 0x11,   // 文件分块数据
    FILE_TRANSFER_COMPLETE = 0x12, // 传输完成确认
    FILE_TRANSFER_CANCEL = 0x13,   // 取消传输
};

// 文件开始通知（发送方 → 接收方）
struct FileTransferStart {
    std::string file_hash;     // SHA256（从 FileChunk::file_hash 获取）
    std::string file_name;     // 原始文件名
    uint32_t    total_chunks;  // 总分块数
    uint64_t    file_size;     // 文件总字节数

    // ...你的序列化代码...
};

// 文件分块消息（发送方 → 接收方）
// → 直接用 FileChunk::serialize() 的结果作为 payload
```

### 关键协议细节

`FileChunk::serialize()` 输出的二进制格式（v1）：

```
[4B magic "LANC"] [1B version=1] [4B chunk_id] [4B total_chunks]
[4B data_size] [4B CRC32 of data] [4B hash_len] [N B SHA256 hex] [M B data]
```

- **魔数 `0x4C414E43`**：如果收到的前 4 字节不是这个，说明不是文件分块
- **CRC32**：`deserialize()` 自动校验，损坏的数据返回 `false`
- **SHA256**：`file_hash` 是 64 字符十六进制小写字符串，用于唯一标识文件

### 接收方协议处理伪代码

```cpp
void handle_message(const std::vector<uint8_t>& raw) {
    MessageType type = peek_type(raw);

    switch (type) {
    case MessageType::FILE_TRANSFER_START: {
        auto start = parse_start(raw);
        fm.createTransferSession(start.file_hash, start.file_name,
                                 start.total_chunks);
        break;
    }
    case MessageType::FILE_TRANSFER_CHUNK: {
        // payload 就是 FileChunk::serialize() 的输出
        auto payload = extract_payload(raw);
        FileChunk chunk;
        if (chunk.deserialize(payload)) {
            fm.addChunkToSession(chunk.file_hash, chunk.chunk_id, chunk.data);
        }
        break;
    }
    case MessageType::FILE_TRANSFER_COMPLETE: {
        auto complete = parse_complete(raw);
        fm.completeTransfer(complete.file_hash);
        break;
    }
    case MessageType::FILE_TRANSFER_CANCEL: {
        auto cancel = parse_cancel(raw);
        fm.cancelTransfer(cancel.file_hash);
        break;
    }
    }
}
```

---

## 给 Module 4（UI / 交互）开发者

你的职责：展示传输进度、文件列表，让用户操作。Module 3 提供所有查询接口。

### 你需要的全部 API

```cpp
#include "file_manager.h"

FileManager fm("./downloads");  // 全局唯一实例即可

// ---- 查询 ----

// 所有正在传输的文件哈希列表
auto sessions = fm.getActiveSessions();  // → vector<string>

// 某个文件的进度（0–100，不存在返回 -1）
int pct = fm.getSessionProgress(file_hash);

// 某个文件是否传完
bool done = fm.isTransferComplete(file_hash);

// 活跃会话总数
size_t count = fm.sessionCount();

// ---- 操作 ----

// 用户点了"取消"
fm.cancelTransfer(file_hash);

// ---- 辅助 ----

// 下载目录在哪
std::string dir = fm.getDownloadDirectory();
```

### UI 轮询示例

```cpp
// 在你的 UI 主循环里（比如每 200ms 调一次）
void update_transfer_list() {
    auto sessions = fm.getActiveSessions();

    for (const auto& hash : sessions) {
        int pct = fm.getSessionProgress(hash);
        // hash 是 SHA256，显示时截短就行
        std::string short_hash = hash.substr(0, 12) + "...";

        if (fm.isTransferComplete(hash)) {
            fm.completeTransfer(hash);  // 写入磁盘，自动清理会话
            show_notification("文件接收完成！");
        } else {
            draw_progress_bar(short_hash, pct);
        }
    }
}
```

### 发送文件的 UI 流程

```
用户点击"发送文件" → 文件选择框 → 你拿到文件路径
    ↓
auto chunks = fm.readAndChunkFile(选中的文件路径);
    ↓
把 chunks 通过 Module 2 → Module 1 发出去
    ↓
（发送进度 = 已发块数 / 总块数，你自己算就行）
```

### 接收文件的 UI 流程

```
Module 1 收到 FILE_TRANSFER_START → Module 2 解析 → 你拿到 file_hash
    ↓
fm.createTransferSession(file_hash, 文件名, total_chunks);
    ↓
之后每收到一个 chunk，Module 2 调用 fm.addChunkToSession(...)
    ↓
UI 定时轮询 fm.getSessionProgress(file_hash) 更新进度条
    ↓
fm.isTransferComplete(file_hash) == true → fm.completeTransfer(file_hash)
```

---

## 快速验证

不确定集成是否正常？运行测试：

```bash
mkdir build && cd build
cmake .. && cmake --build .
./test
# 输出 "✓ All 12 tests passed!" 就说明一切正常
```

---

## 有问题？

- 完整 API 文档 → [API_GUIDE.md](./API_GUIDE.md)
- 可运行的代码示例 → [example.cpp](./example.cpp)
- 头文件即文档 → [file_manager.h](./file_manager.h)（每个方法都有注释）

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <cstdint>
#include <chrono>

// ============================================================================
//  Module 3 — 文件与存储管理
//
//  用法速查（其他模块开发者看这里就够了）：
//
//  发送：auto chunks = FileManager("./downloads").readAndChunkFile("f.bin");
//        for (auto& c : chunks) { socket_send(c.serialize()); }
//
//  接收：FileManager fm;  fm.createTransferSession(hash, name, total);
//        FileChunk c;  if (c.deserialize(wire)) fm.addChunkToSession(...);
//        if (fm.isTransferComplete(hash)) fm.completeTransfer(hash);
//
//  进度：fm.getSessionProgress(hash) → 0-100
//
//  协议：[4B magic LANC] [1B ver] [4B id] [4B total] [4B size] [4B CRC32]
//        [4B hash_len] [64B SHA256] [data]
//
//  主要类：SHA256 / FileChunk / FileTransferSession / FileManager
// ============================================================================

// ============================================================================
//  SHA256 — 独立实现，零外部依赖
// ============================================================================

/**
 * @class SHA256
 * @brief 独立的 SHA-256 哈希计算器
 *
 * 纯 C++ 实现，不依赖 OpenSSL 或其他第三方库。
 * 用法：SHA256().update(data).digest() → 64 字符十六进制字符串
 */
class SHA256 {
public:
    SHA256();

    /** 追加数据（可多次调用） */
    SHA256& update(const void* data, size_t length);
    SHA256& update(const std::string& str);
    SHA256& update(const std::vector<uint8_t>& data);

    /** 输出 64 字符十六进制小写摘要 */
    std::string digest() const;

    /** 输出 32 字节原始二进制摘要 */
    std::vector<uint8_t> rawDigest() const;

    /** 便捷静态方法：对文件路径计算 SHA256 */
    static std::string hashFile(const std::string& file_path);

    /** 便捷静态方法：对内存数据计算 SHA256 */
    static std::string hashData(const void* data, size_t length);

private:
    void transform(const uint8_t block[64]);
    void finalize();

    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[64];
    size_t   buffer_offset;
    bool     finalized;
};

// ============================================================================
//  CRC32
// ============================================================================

/**
 * @brief 计算一段数据的 CRC32 校验值
 * @param data 数据指针
 * @param length 数据长度
 * @return 32 位 CRC 值
 */
uint32_t crc32(const void* data, size_t length);

inline uint32_t crc32(const std::vector<uint8_t>& data) {
    return crc32(data.data(), data.size());
}

// ============================================================================
//  协议常量
// ============================================================================

/// 序列化魔数 "LANC" — 用于识别 EGG-Delivery 文件分块
constexpr uint32_t PROTOCOL_MAGIC  = 0x4C414E43;
/// 当前协议版本
constexpr uint8_t  PROTOCOL_VERSION = 1;

// ============================================================================
//  FileChunk — 文件分块单元
// ============================================================================

/**
 * @class FileChunk
 * @brief 文件的一个分块，可独立序列化后在网络上传输
 *
 * 序列化格式（version 1）：
 *   [4B magic] [1B version] [4B chunk_id] [4B total_chunks]
 *   [4B data_size] [4B crc32] [4B hash_len] [N B hash] [M B data]
 */
class FileChunk {
public:
    uint32_t chunk_id      = 0;
    uint32_t total_chunks  = 0;
    std::string file_hash;              ///< 文件 SHA256（64 字符十六进制）
    std::vector<uint8_t> data;
    uint32_t data_size     = 0;

    FileChunk() = default;

    /**
     * @brief 序列化为网络传输字节流（含魔数 + 版本 + CRC32）
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief 从网络字节流反序列化
     * @param buffer 收到的原始字节
     * @return true=成功且 CRC32 校验通过，false=格式错误或数据损坏
     */
    bool deserialize(const std::vector<uint8_t>& buffer);
};

// ============================================================================
//  FileTransferSession — 单个文件接收会话
// ============================================================================

/**
 * @class FileTransferSession
 * @brief 管理一个文件的完整接收过程
 *
 * 特性：
 * - 支持乱序分块到达
 * - 实时进度百分比
 * - 空闲超时追踪
 * - 缺失分块查询（为断点续传预留）
 */
class FileTransferSession {
public:
    FileTransferSession(const std::string& hash,
                        const std::string& save_path,
                        uint32_t total_chunks);

    // ---- 分块管理 ----

    /** 添加一个已接收的分块，chunk_id 越界返回 false */
    bool addChunk(uint32_t chunk_id, const std::vector<uint8_t>& data);

    /** 所有分块是否已到齐 */
    bool isComplete() const;

    /** 已接收分块数量 */
    uint32_t getReceivedChunkCount() const;

    /** 传输进度 0–100 */
    uint32_t getProgressPercentage() const;

    /** 按序将所有分块写入磁盘，未完成时调用返回 false */
    bool assembleFile();

    /** 释放内存中的分块缓存（组装后建议调用） */
    void clearCache();

    // ---- 断点续传支持 ----

    /** 返回所有尚未收到的分块 ID 列表 */
    std::vector<uint32_t> getMissingChunks() const;

    // ---- 空闲追踪 ----

    /** 更新最后活跃时间（每次 addChunk 自动调用） */
    void touch();

    /** 距上次活跃过去了多少秒 */
    double secondsIdle() const;

    // ---- 属性 ----

    const std::string& fileHash()    const { return file_hash_; }
    const std::string& savePath()    const { return save_path_; }
    uint32_t           totalChunks() const { return total_chunks_; }

private:
    std::string file_hash_;
    std::string save_path_;
    uint32_t    total_chunks_;
    std::map<uint32_t, std::vector<uint8_t>> chunks_;
    std::vector<bool> received_flags_;

    std::chrono::steady_clock::time_point last_active_;
};

// ============================================================================
//  FileManager — 文件管理器（核心门面）
// ============================================================================

/**
 * @class FileManager
 * @brief 文件全生命周期管理：分块、传输、组装、清理
 *
 * 这是 Module 3 对外暴露的唯一入口。其他模块只需包含此头文件，
 * 创建一个 FileManager 实例即可使用全部文件传输功能。
 */
class FileManager {
public:
    static constexpr uint32_t DEFAULT_CHUNK_SIZE = 64 * 1024;   // 64 KB

    /**
     * @param download_dir 接收文件的保存目录，自动创建
     */
    explicit FileManager(const std::string& download_dir = "./downloads");
    ~FileManager();

    // ---- 目录管理 ----

    bool        setDownloadDirectory(const std::string& dir);
    std::string getDownloadDirectory() const;

    // ---- 发送端 API ----

    /**
     * @brief 读取本地文件并分块
     * @param file_path 文件路径
     * @param chunk_size 分块大小（字节），默认 64KB
     * @return 分块列表，文件不存在时为空
     */
    std::vector<FileChunk> readAndChunkFile(
        const std::string& file_path,
        uint32_t chunk_size = DEFAULT_CHUNK_SIZE);

    // ---- 接收端 API ----

    /**
     * @brief 为即将到来的文件传输创建接收会话
     * @return false 表示该 file_hash 的会话已存在
     */
    bool createTransferSession(const std::string& file_hash,
                               const std::string& file_name,
                               uint32_t total_chunks);

    /** 向会话添加一个分块 */
    bool addChunkToSession(const std::string& file_hash,
                           uint32_t chunk_id,
                           const std::vector<uint8_t>& chunk_data);

    /** 查询进度 0–100，会话不存在返回 -1 */
    int32_t getSessionProgress(const std::string& file_hash) const;

    /** 所有分块是否到齐 */
    bool isTransferComplete(const std::string& file_hash) const;

    /** 组装文件并写入磁盘，成功后自动销毁会话 */
    bool completeTransfer(const std::string& file_hash);

    /** 取消传输并丢弃所有已收分块 */
    bool cancelTransfer(const std::string& file_hash);

    // ---- 会话管理 ----

    /** 所有活跃会话的文件哈希列表 */
    std::vector<std::string> getActiveSessions() const;

    /**
     * @brief 清理超过 max_idle_seconds 未活动的会话
     * @param max_idle_seconds 最大空闲秒数，默认 3600（1 小时）
     * @return 清理掉的会话数
     */
    size_t cleanupIdleSessions(uint32_t max_idle_seconds = 3600);

    /** 活跃会话总数 */
    size_t sessionCount() const;

private:
    std::string download_dir_;
    std::map<std::string, FileTransferSession> sessions_;
};

#endif // FILE_MANAGER_H

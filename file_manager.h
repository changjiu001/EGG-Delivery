#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <cstdint>

/**
 * @class FileChunk
 * @brief 文件分块类 - 表示文件的一个分块单元
 */
class FileChunk {
public:
    uint32_t chunk_id;           // 分块ID
    uint32_t total_chunks;       // 总分块数
    std::string file_hash;       // 文件哈希值（用于验证完整性）
    std::vector<uint8_t> data;   // 分块数据
    uint32_t data_size;          // 实际数据大小

    FileChunk() : chunk_id(0), total_chunks(0), data_size(0) {}
    
    /**
     * @brief 序列化分块为字节流
     * @return 序列化后的字节向量
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief 从字节流反序列化分块
     * @param buffer 输入字节流
     * @return 反序列化是否成功
     */
    bool deserialize(const std::vector<uint8_t>& buffer);
};

/**
 * @class FileTransferSession
 * @brief 文件传输会话类 - 管理单个文件的接收过程
 */
class FileTransferSession {
private:
    std::string file_hash;                           // 文件哈希ID
    std::string save_path;                           // 保存路径
    uint32_t total_chunks;                           // 总分块数
    std::map<uint32_t, std::vector<uint8_t>> chunks; // 已接收的分块缓存
    std::vector<bool> received_flags;                // 各分块接收状态

public:
    FileTransferSession(const std::string& hash, const std::string& path, uint32_t total);
    
    /**
     * @brief 添加已接收的分块
     * @param chunk_id 分块ID
     * @param data 分块数据
     * @return 添加是否成功
     */
    bool addChunk(uint32_t chunk_id, const std::vector<uint8_t>& data);
    
    /**
     * @brief 检查文件是否完全接收
     * @return 是否完成
     */
    bool isComplete() const;
    
    /**
     * @brief 获取已接收的分块数
     * @return 分块数量
     */
    uint32_t getReceivedChunkCount() const;
    
    /**
     * @brief 获取传输进度（百分比）
     * @return 0-100之间的进度值
     */
    uint32_t getProgressPercentage() const;
    
    /**
     * @brief 将所有分块组装成完整文件
     * @return 组装是否成功
     */
    bool assembleFile();
    
    /**
     * @brief 清理缓存
     */
    void clearCache();
};

/**
 * @class FileManager
 * @brief 文件管理器类 - 负责文件的读写、分块和传输管理
 */
class FileManager {
private:
    static const uint32_t DEFAULT_CHUNK_SIZE = 64 * 1024;  // 默认分块大小 64KB
    std::string download_dir;                               // 下载目录
    std::map<std::string, FileTransferSession> sessions;    // 传输会话管理

public:
    explicit FileManager(const std::string& dir = "./downloads");
    
    ~FileManager();
    
    /**
     * @brief 设置下载目录
     * @param dir 目录路径
     * @return 设置是否成功
     */
    bool setDownloadDirectory(const std::string& dir);
    
    /**
     * @brief 获取下载目录
     * @return 下载目录路径
     */
    std::string getDownloadDirectory() const;
    
    /**
     * @brief 读取文件并分块
     * @param file_path 文件路径
     * @param chunk_size 单个分块大小（字节）
     * @return 分块列表
     */
    std::vector<FileChunk> readAndChunkFile(
        const std::string& file_path,
        uint32_t chunk_size = DEFAULT_CHUNK_SIZE
    );
    
    /**
     * @brief 创建新的文件传输会话（用于接收文件）
     * @param file_hash 文件唯一标识
     * @param file_name 文件名
     * @param total_chunks 总分块数
     * @return 创建是否成功
     */
    bool createTransferSession(
        const std::string& file_hash,
        const std::string& file_name,
        uint32_t total_chunks
    );
    
    /**
     * @brief 向传输会话添加分块
     * @param file_hash 文件标识
     * @param chunk_id 分块ID
     * @param chunk_data 分块数据
     * @return 添加是否成功
     */
    bool addChunkToSession(
        const std::string& file_hash,
        uint32_t chunk_id,
        const std::vector<uint8_t>& chunk_data
    );
    
    /**
     * @brief 获取传输会话的进度
     * @param file_hash 文件标识
     * @return 进度百分比 (-1 表示会话不存在)
     */
    int32_t getSessionProgress(const std::string& file_hash) const;
    
    /**
     * @brief 检查文件传输是否完成
     * @param file_hash 文件标识
     * @return 是否完成
     */
    bool isTransferComplete(const std::string& file_hash) const;
    
    /**
     * @brief 完成文件传输（组装并保存）
     * @param file_hash 文件标识
     * @return 完成是否成功
     */
    bool completeTransfer(const std::string& file_hash);
    
    /**
     * @brief 取消传输会话
     * @param file_hash 文件标识
     * @return 取消是否成功
     */
    bool cancelTransfer(const std::string& file_hash);
    
    /**
     * @brief 获取会话列表
     * @return 所有活跃会话的文件哈希列表
     */
    std::vector<std::string> getActiveSessions() const;
    
    /**
     * @brief 清理过期的会话
     * @param max_idle_time 最大空闲时间（秒）
     */
    void cleanupIdleSessions(uint32_t max_idle_time = 3600);
    
    /**
     * @brief 获取文件的MD5哈希值
     * @param file_path 文件路径
     * @return MD5哈希字符串
     */
    static std::string calculateFileHash(const std::string& file_path);
};

#endif // FILE_MANAGER_H

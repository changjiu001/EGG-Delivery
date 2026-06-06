#include "file_manager.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <iomanip>

namespace fs = std::filesystem;

// ============== FileChunk 实现 ==============

std::vector<uint8_t> FileChunk::serialize() const {
    std::vector<uint8_t> buffer;
    
    // 写入分块ID (4字节)
    buffer.push_back((chunk_id >> 24) & 0xFF);
    buffer.push_back((chunk_id >> 16) & 0xFF);
    buffer.push_back((chunk_id >> 8) & 0xFF);
    buffer.push_back(chunk_id & 0xFF);
    
    // 写入总分块数 (4字节)
    buffer.push_back((total_chunks >> 24) & 0xFF);
    buffer.push_back((total_chunks >> 16) & 0xFF);
    buffer.push_back((total_chunks >> 8) & 0xFF);
    buffer.push_back(total_chunks & 0xFF);
    
    // 写入数据大小 (4字节)
    buffer.push_back((data_size >> 24) & 0xFF);
    buffer.push_back((data_size >> 16) & 0xFF);
    buffer.push_back((data_size >> 8) & 0xFF);
    buffer.push_back(data_size & 0xFF);
    
    // 写入哈希字符串长度 (4字节)
    uint32_t hash_len = file_hash.length();
    buffer.push_back((hash_len >> 24) & 0xFF);
    buffer.push_back((hash_len >> 16) & 0xFF);
    buffer.push_back((hash_len >> 8) & 0xFF);
    buffer.push_back(hash_len & 0xFF);
    
    // 写入哈希字符串
    buffer.insert(buffer.end(), file_hash.begin(), file_hash.end());
    
    // 写入数据
    buffer.insert(buffer.end(), data.begin(), data.end());
    
    return buffer;
}

bool FileChunk::deserialize(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 16) {
        return false;
    }
    
    size_t offset = 0;
    
    // 读取分块ID
    chunk_id = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
               (buffer[offset + 2] << 8) | buffer[offset + 3];
    offset += 4;
    
    // 读取总分块数
    total_chunks = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                   (buffer[offset + 2] << 8) | buffer[offset + 3];
    offset += 4;
    
    // 读取数据大小
    data_size = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                (buffer[offset + 2] << 8) | buffer[offset + 3];
    offset += 4;
    
    // 读取哈希长度
    uint32_t hash_len = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
    offset += 4;
    
    // 读取哈希字符串
    if (offset + hash_len > buffer.size()) {
        return false;
    }
    file_hash.assign(buffer.begin() + offset, buffer.begin() + offset + hash_len);
    offset += hash_len;
    
    // 读取数据
    if (offset + data_size > buffer.size()) {
        return false;
    }
    data.assign(buffer.begin() + offset, buffer.begin() + offset + data_size);
    
    return true;
}

// ============== FileTransferSession 实现 ==============

FileTransferSession::FileTransferSession(
    const std::string& hash,
    const std::string& path,
    uint32_t total)
    : file_hash(hash), save_path(path), total_chunks(total) {
    received_flags.resize(total_chunks, false);
}

bool FileTransferSession::addChunk(uint32_t chunk_id, const std::vector<uint8_t>& data) {
    if (chunk_id >= total_chunks) {
        return false;
    }
    
    chunks[chunk_id] = data;
    received_flags[chunk_id] = true;
    return true;
}

bool FileTransferSession::isComplete() const {
    for (bool flag : received_flags) {
        if (!flag) {
            return false;
        }
    }
    return true;
}

uint32_t FileTransferSession::getReceivedChunkCount() const {
    uint32_t count = 0;
    for (bool flag : received_flags) {
        if (flag) count++;
    }
    return count;
}

uint32_t FileTransferSession::getProgressPercentage() const {
    if (total_chunks == 0) return 0;
    return (getReceivedChunkCount() * 100) / total_chunks;
}

bool FileTransferSession::assembleFile() {
    if (!isComplete()) {
        std::cerr << "Cannot assemble incomplete file. Progress: "
                  << getProgressPercentage() << "%" << std::endl;
        return false;
    }
    
    std::ofstream out_file(save_path, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "Failed to open output file: " << save_path << std::endl;
        return false;
    }
    
    // 按分块顺序写入文件
    for (uint32_t i = 0; i < total_chunks; ++i) {
        auto it = chunks.find(i);
        if (it != chunks.end()) {
            out_file.write(reinterpret_cast<const char*>(it->second.data()),
                          it->second.size());
        }
    }
    
    out_file.close();
    std::cout << "File successfully assembled: " << save_path << std::endl;
    return true;
}

void FileTransferSession::clearCache() {
    chunks.clear();
}

// ============== FileManager 实现 ==============

FileManager::FileManager(const std::string& dir) : download_dir(dir) {
    // 创建下载目录
    try {
        fs::create_directories(download_dir);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create download directory: " << e.what() << std::endl;
    }
}

FileManager::~FileManager() {
    sessions.clear();
}

bool FileManager::setDownloadDirectory(const std::string& dir) {
    try {
        fs::create_directories(dir);
        download_dir = dir;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to set download directory: " << e.what() << std::endl;
        return false;
    }
}

std::string FileManager::getDownloadDirectory() const {
    return download_dir;
}

std::vector<FileChunk> FileManager::readAndChunkFile(
    const std::string& file_path,
    uint32_t chunk_size) {
    
    std::vector<FileChunk> result;
    
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return result;
    }
    
    // 获取文件大小
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // 计算总分块数
    uint32_t total_chunks = (file_size + chunk_size - 1) / chunk_size;
    std::string file_hash = calculateFileHash(file_path);
    
    std::cout << "Reading file: " << file_path << std::endl;
    std::cout << "File size: " << file_size << " bytes" << std::endl;
    std::cout << "Total chunks: " << total_chunks << std::endl;
    
    // 逐块读取文件
    for (uint32_t i = 0; i < total_chunks; ++i) {
        FileChunk chunk;
        chunk.chunk_id = i;
        chunk.total_chunks = total_chunks;
        chunk.file_hash = file_hash;
        
        // 读取分块数据
        std::vector<uint8_t> buffer(chunk_size);
        file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
        std::streamsize bytes_read = file.gcount();
        
        chunk.data.assign(buffer.begin(), buffer.begin() + bytes_read);
        chunk.data_size = bytes_read;
        
        result.push_back(chunk);
        
        std::cout << "Chunk " << i + 1 << "/" << total_chunks << " ready ("
                  << bytes_read << " bytes)" << std::endl;
    }
    
    file.close();
    return result;
}

bool FileManager::createTransferSession(
    const std::string& file_hash,
    const std::string& file_name,
    uint32_t total_chunks) {
    
    if (sessions.find(file_hash) != sessions.end()) {
        std::cerr << "Session already exists for file: " << file_hash << std::endl;
        return false;
    }
    
    std::string save_path = download_dir + "/" + file_name;
    sessions.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(file_hash),
        std::forward_as_tuple(file_hash, save_path, total_chunks)
    );
    
    std::cout << "Transfer session created for: " << file_name << " (ID: " << file_hash << ")" << std::endl;
    return true;
}

bool FileManager::addChunkToSession(
    const std::string& file_hash,
    uint32_t chunk_id,
    const std::vector<uint8_t>& chunk_data) {
    
    auto it = sessions.find(file_hash);
    if (it == sessions.end()) {
        std::cerr << "Session not found: " << file_hash << std::endl;
        return false;
    }
    
    return it->second.addChunk(chunk_id, chunk_data);
}

int32_t FileManager::getSessionProgress(const std::string& file_hash) const {
    auto it = sessions.find(file_hash);
    if (it == sessions.end()) {
        return -1;
    }
    return it->second.getProgressPercentage();
}

bool FileManager::isTransferComplete(const std::string& file_hash) const {
    auto it = sessions.find(file_hash);
    if (it == sessions.end()) {
        return false;
    }
    return it->second.isComplete();
}

bool FileManager::completeTransfer(const std::string& file_hash) {
    auto it = sessions.find(file_hash);
    if (it == sessions.end()) {
        std::cerr << "Session not found: " << file_hash << std::endl;
        return false;
    }
    
    bool result = it->second.assembleFile();
    if (result) {
        sessions.erase(it);
    }
    return result;
}

bool FileManager::cancelTransfer(const std::string& file_hash) {
    auto it = sessions.find(file_hash);
    if (it == sessions.end()) {
        return false;
    }
    
    it->second.clearCache();
    sessions.erase(it);
    std::cout << "Transfer cancelled: " << file_hash << std::endl;
    return true;
}

std::vector<std::string> FileManager::getActiveSessions() const {
    std::vector<std::string> result;
    for (const auto& pair : sessions) {
        result.push_back(pair.first);
    }
    return result;
}

void FileManager::cleanupIdleSessions(uint32_t max_idle_time) {
    // TODO: 在实际应用中，可以使用时间戳来跟踪会话的空闲时间
    std::cout << "Session cleanup called (max idle: " << max_idle_time << " seconds)" << std::endl;
}

std::string FileManager::calculateFileHash(const std::string& file_path) {
    // 简化版本：使用文件名和大小的组合作为哈希
    // 在生产环境中应使用真正的MD5或SHA256
    try {
        auto file_size = fs::file_size(file_path);
        std::stringstream ss;
        ss << std::hex << std::hash<std::string>()(file_path) << "_" << file_size;
        return ss.str();
    } catch (const std::exception& e) {
        std::cerr << "Error calculating file hash: " << e.what() << std::endl;
        return "unknown_hash";
    }
}

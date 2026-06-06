#include "file_manager.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace fs = std::filesystem;

// ============================================================================
//  SHA256 实现
// ============================================================================

namespace {

constexpr uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t sha_ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t sha_maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t sha_S0(uint32_t x) {
    return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

inline uint32_t sha_S1(uint32_t x) {
    return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

inline uint32_t sha_s0(uint32_t x) {
    return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}

inline uint32_t sha_s1(uint32_t x) {
    return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

} // anonymous namespace

SHA256::SHA256() {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    bit_count    = 0;
    buffer_offset = 0;
    finalized    = false;
}

SHA256& SHA256::update(const void* data, size_t length) {
    if (finalized) return *this;

    const auto* bytes = static_cast<const uint8_t*>(data);
    bit_count += length * 8;

    // 如果 buffer 里有残留数据，先填满 buffer 再 transform
    if (buffer_offset > 0) {
        size_t fill = 64 - buffer_offset;
        if (length < fill) {
            std::memcpy(buffer + buffer_offset, bytes, length);
            buffer_offset += length;
            return *this;
        }
        std::memcpy(buffer + buffer_offset, bytes, fill);
        transform(buffer);
        bytes   += fill;
        length  -= fill;
        buffer_offset = 0;
    }

    // 处理完整 64 字节块
    while (length >= 64) {
        transform(bytes);
        bytes  += 64;
        length -= 64;
    }

    // 剩余不足 64 字节的存入 buffer
    if (length > 0) {
        std::memcpy(buffer, bytes, length);
        buffer_offset = length;
    }

    return *this;
}

SHA256& SHA256::update(const std::string& str) {
    return update(str.data(), str.size());
}

SHA256& SHA256::update(const std::vector<uint8_t>& data) {
    return update(data.data(), data.size());
}

void SHA256::transform(const uint8_t block[64]) {
    uint32_t w[64];

    // 前 16 个字直接来自 block（大端）
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4])     << 24) |
               (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8)  |
               (uint32_t(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = sha_s1(w[i - 2]) + w[i - 7] + sha_s0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = h + sha_S1(e) + sha_ch(e, f, g) + SHA256_K[i] + w[i];
        uint32_t T2 = sha_S0(a) + sha_maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256::finalize() {
    if (finalized) return;
    finalized = true;

    // Padding: append 0x80, then zeros, then 64-bit length in big-endian
    uint64_t bits = bit_count;

    buffer[buffer_offset++] = 0x80;

    if (buffer_offset > 56) {
        // 空间不够放长度，填零再 transform
        std::memset(buffer + buffer_offset, 0, 64 - buffer_offset);
        transform(buffer);
        buffer_offset = 0;
    }

    std::memset(buffer + buffer_offset, 0, 56 - buffer_offset);

    // 64-bit big-endian length
    for (int i = 0; i < 8; ++i) {
        buffer[56 + i] = (bits >> (56 - i * 8)) & 0xFF;
    }

    transform(buffer);
}

std::string SHA256::digest() const {
    // mutable finalize
    SHA256& self = const_cast<SHA256&>(*this);
    self.finalize();

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(8) << self.state[i];
    }
    return ss.str();
}

std::vector<uint8_t> SHA256::rawDigest() const {
    SHA256& self = const_cast<SHA256&>(*this);
    self.finalize();

    std::vector<uint8_t> result(32);
    for (int i = 0; i < 8; ++i) {
        result[i * 4]     = (self.state[i] >> 24) & 0xFF;
        result[i * 4 + 1] = (self.state[i] >> 16) & 0xFF;
        result[i * 4 + 2] = (self.state[i] >> 8)  & 0xFF;
        result[i * 4 + 3] =  self.state[i]        & 0xFF;
    }
    return result;
}

std::string SHA256::hashFile(const std::string& file_path) {
    SHA256 sha;
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    char buf[8192];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        sha.update(buf, file.gcount());
    }
    return sha.digest();
}

std::string SHA256::hashData(const void* data, size_t length) {
    SHA256 sha;
    sha.update(data, length);
    return sha.digest();
}

// ============================================================================
//  CRC32 实现
// ============================================================================

namespace {

constexpr uint32_t CRC32_POLY = 0xEDB88320;

uint32_t crc32_table[256];
bool     crc32_table_ready = false;

void crc32_init_table() {
    if (crc32_table_ready) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLY : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_ready = true;
}

} // anonymous namespace

uint32_t crc32(const void* data, size_t length) {
    crc32_init_table();
    uint32_t crc = 0xFFFFFFFF;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
//  FileChunk 实现
// ============================================================================

// ---- 序列化辅助宏 ----

namespace {

inline void write_u32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8)  & 0xFF);
    buf.push_back( val        & 0xFF);
}

inline uint32_t read_u32(const std::vector<uint8_t>& buf, size_t& offset) {
    uint32_t val = (uint32_t(buf[offset])     << 24) |
                   (uint32_t(buf[offset + 1]) << 16) |
                   (uint32_t(buf[offset + 2]) << 8)  |
                   (uint32_t(buf[offset + 3]));
    offset += 4;
    return val;
}

} // anonymous namespace

std::vector<uint8_t> FileChunk::serialize() const {
    std::vector<uint8_t> buf;

    // 计算数据 CRC32
    uint32_t data_crc = crc32(data.data(), data.size());

    // 魔数 (4B)
    write_u32(buf, PROTOCOL_MAGIC);

    // 版本 (1B)
    buf.push_back(PROTOCOL_VERSION);

    // chunk_id (4B)
    write_u32(buf, chunk_id);

    // total_chunks (4B)
    write_u32(buf, total_chunks);

    // data_size (4B)
    write_u32(buf, static_cast<uint32_t>(data.size()));

    // CRC32 of data (4B)
    write_u32(buf, data_crc);

    // hash_len (4B)
    uint32_t hash_len = static_cast<uint32_t>(file_hash.size());
    write_u32(buf, hash_len);

    // hash (N B)
    buf.insert(buf.end(), file_hash.begin(), file_hash.end());

    // data (M B)
    buf.insert(buf.end(), data.begin(), data.end());

    return buf;
}

bool FileChunk::deserialize(const std::vector<uint8_t>& buf) {
    // 最小长度：magic(4) + version(1) + chunk_id(4) + total_chunks(4)
    //          + data_size(4) + crc32(4) + hash_len(4) = 25
    if (buf.size() < 25) {
        return false;
    }

    size_t off = 0;

    // 魔数
    uint32_t magic = read_u32(buf, off);
    if (magic != PROTOCOL_MAGIC) {
        return false;   // 不是我们的协议
    }

    // 版本
    uint8_t version = buf[off++];
    if (version != PROTOCOL_VERSION) {
        return false;   // 不支持的协议版本
    }

    // chunk_id
    chunk_id = read_u32(buf, off);

    // total_chunks
    total_chunks = read_u32(buf, off);

    // data_size
    data_size = read_u32(buf, off);

    // crc32
    uint32_t expected_crc = read_u32(buf, off);

    // hash_len
    uint32_t hash_len = read_u32(buf, off);

    // hash
    if (off + hash_len > buf.size()) {
        return false;
    }
    file_hash.assign(buf.begin() + off, buf.begin() + off + hash_len);
    off += hash_len;

    // data
    if (off + data_size > buf.size()) {
        return false;
    }
    data.assign(buf.begin() + off, buf.begin() + off + data_size);

    // CRC32 校验
    uint32_t actual_crc = crc32(data.data(), data.size());
    if (actual_crc != expected_crc) {
        std::cerr << "[FileChunk] CRC32 mismatch! expected=0x"
                  << std::hex << expected_crc
                  << " actual=0x" << actual_crc << std::dec << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
//  FileTransferSession 实现
// ============================================================================

FileTransferSession::FileTransferSession(
    const std::string& hash,
    const std::string& path,
    uint32_t total)
    : file_hash_(hash), save_path_(path), total_chunks_(total) {
    received_flags_.resize(total_chunks_, false);
    last_active_ = std::chrono::steady_clock::now();
}

bool FileTransferSession::addChunk(uint32_t chunk_id, const std::vector<uint8_t>& data) {
    if (chunk_id >= total_chunks_) {
        return false;
    }
    chunks_[chunk_id] = data;
    received_flags_[chunk_id] = true;
    last_active_ = std::chrono::steady_clock::now();
    return true;
}

bool FileTransferSession::isComplete() const {
    for (bool flag : received_flags_) {
        if (!flag) return false;
    }
    return true;
}

uint32_t FileTransferSession::getReceivedChunkCount() const {
    uint32_t count = 0;
    for (bool flag : received_flags_) {
        if (flag) ++count;
    }
    return count;
}

uint32_t FileTransferSession::getProgressPercentage() const {
    if (total_chunks_ == 0) return 0;
    return (getReceivedChunkCount() * 100) / total_chunks_;
}

bool FileTransferSession::assembleFile() {
    if (!isComplete()) {
        std::cerr << "[FileTransferSession] Cannot assemble incomplete file. "
                  << getReceivedChunkCount() << "/" << total_chunks_
                  << " chunks received." << std::endl;
        return false;
    }

    // 确保父目录存在
    fs::path file_path(save_path_);
    try {
        fs::create_directories(file_path.parent_path());
    } catch (const std::exception& e) {
        std::cerr << "[FileTransferSession] Failed to create directory: "
                  << e.what() << std::endl;
        return false;
    }

    std::ofstream out(save_path_, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[FileTransferSession] Failed to open output file: "
                  << save_path_ << std::endl;
        return false;
    }

    // 按序写入
    for (uint32_t i = 0; i < total_chunks_; ++i) {
        auto it = chunks_.find(i);
        if (it != chunks_.end()) {
            out.write(reinterpret_cast<const char*>(it->second.data()),
                      it->second.size());
        }
    }
    out.close();

    // 校验文件完整性
    std::string actual_hash = SHA256::hashFile(save_path_);
    if (actual_hash != file_hash_) {
        std::cerr << "[FileTransferSession] ⚠ SHA256 mismatch!\n"
                  << "  Expected: " << file_hash_ << "\n"
                  << "  Actual:   " << actual_hash << std::endl;
        // 不删除文件，让调用者决定
    } else {
        std::cout << "[FileTransferSession] ✓ SHA256 verified: "
                  << save_path_ << std::endl;
    }

    std::cout << "[FileTransferSession] File assembled: "
              << save_path_ << std::endl;
    return true;
}

void FileTransferSession::clearCache() {
    chunks_.clear();
}

std::vector<uint32_t> FileTransferSession::getMissingChunks() const {
    std::vector<uint32_t> missing;
    for (uint32_t i = 0; i < total_chunks_; ++i) {
        if (!received_flags_[i]) {
            missing.push_back(i);
        }
    }
    return missing;
}

void FileTransferSession::touch() {
    last_active_ = std::chrono::steady_clock::now();
}

double FileTransferSession::secondsIdle() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - last_active_).count();
}

// ============================================================================
//  FileManager 实现
// ============================================================================

FileManager::FileManager(const std::string& dir) : download_dir_(dir) {
    try {
        fs::create_directories(download_dir_);
    } catch (const std::exception& e) {
        std::cerr << "[FileManager] Failed to create download directory: "
                  << e.what() << std::endl;
    }
}

FileManager::~FileManager() {
    sessions_.clear();
}

bool FileManager::setDownloadDirectory(const std::string& dir) {
    try {
        fs::create_directories(dir);
        download_dir_ = dir;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FileManager] Failed to set download directory: "
                  << e.what() << std::endl;
        return false;
    }
}

std::string FileManager::getDownloadDirectory() const {
    return download_dir_;
}

std::vector<FileChunk> FileManager::readAndChunkFile(
    const std::string& file_path,
    uint32_t chunk_size) {

    std::vector<FileChunk> result;

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[FileManager] Failed to open file: " << file_path << std::endl;
        return result;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 真正的 SHA256
    std::string file_hash = SHA256::hashFile(file_path);
    if (file_hash.empty()) {
        std::cerr << "[FileManager] Failed to compute SHA256 for: " << file_path << std::endl;
        return result;
    }

    uint32_t total_chunks = static_cast<uint32_t>(
        (file_size + chunk_size - 1) / chunk_size);

    std::cout << "[FileManager] Reading: " << file_path << "\n"
              << "  Size: " << file_size << " bytes\n"
              << "  Chunks: " << total_chunks << " × " << chunk_size << " bytes\n"
              << "  SHA256: " << file_hash << std::endl;

    for (uint32_t i = 0; i < total_chunks; ++i) {
        FileChunk chunk;
        chunk.chunk_id     = i;
        chunk.total_chunks = total_chunks;
        chunk.file_hash    = file_hash;

        std::vector<uint8_t> buffer(chunk_size);
        file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
        std::streamsize bytes_read = file.gcount();

        chunk.data.assign(buffer.begin(), buffer.begin() + bytes_read);
        chunk.data_size = static_cast<uint32_t>(bytes_read);

        result.push_back(std::move(chunk));
    }

    file.close();
    return result;
}

bool FileManager::createTransferSession(
    const std::string& file_hash,
    const std::string& file_name,
    uint32_t total_chunks) {

    if (sessions_.find(file_hash) != sessions_.end()) {
        std::cerr << "[FileManager] Session already exists: " << file_hash << std::endl;
        return false;
    }

    std::string save_path = download_dir_ + "/" + file_name;

    sessions_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(file_hash),
        std::forward_as_tuple(file_hash, save_path, total_chunks));

    std::cout << "[FileManager] Session created: " << file_name
              << " (" << total_chunks << " chunks)" << std::endl;
    return true;
}

bool FileManager::addChunkToSession(
    const std::string& file_hash,
    uint32_t chunk_id,
    const std::vector<uint8_t>& chunk_data) {

    auto it = sessions_.find(file_hash);
    if (it == sessions_.end()) {
        std::cerr << "[FileManager] Session not found: " << file_hash << std::endl;
        return false;
    }
    return it->second.addChunk(chunk_id, chunk_data);
}

int32_t FileManager::getSessionProgress(const std::string& file_hash) const {
    auto it = sessions_.find(file_hash);
    if (it == sessions_.end()) {
        return -1;
    }
    return static_cast<int32_t>(it->second.getProgressPercentage());
}

bool FileManager::isTransferComplete(const std::string& file_hash) const {
    auto it = sessions_.find(file_hash);
    if (it == sessions_.end()) {
        return false;
    }
    return it->second.isComplete();
}

bool FileManager::completeTransfer(const std::string& file_hash) {
    auto it = sessions_.find(file_hash);
    if (it == sessions_.end()) {
        std::cerr << "[FileManager] Session not found: " << file_hash << std::endl;
        return false;
    }

    if (!it->second.assembleFile()) {
        return false;
    }

    sessions_.erase(it);
    return true;
}

bool FileManager::cancelTransfer(const std::string& file_hash) {
    auto it = sessions_.find(file_hash);
    if (it == sessions_.end()) {
        return false;
    }

    it->second.clearCache();
    sessions_.erase(it);
    std::cout << "[FileManager] Transfer cancelled: " << file_hash << std::endl;
    return true;
}

std::vector<std::string> FileManager::getActiveSessions() const {
    std::vector<std::string> result;
    for (const auto& pair : sessions_) {
        result.push_back(pair.first);
    }
    return result;
}

size_t FileManager::cleanupIdleSessions(uint32_t max_idle_seconds) {
    size_t removed = 0;
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (it->second.secondsIdle() > max_idle_seconds) {
            std::cout << "[FileManager] Cleaning idle session: "
                      << it->first << " (idle " << it->second.secondsIdle() << "s)"
                      << std::endl;
            it->second.clearCache();
            it = sessions_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

size_t FileManager::sessionCount() const {
    return sessions_.size();
}

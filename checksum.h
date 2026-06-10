#pragma once
#include <cstdint>
#include <string>
#include <vector>

// CRC32 checksum for file integrity verification
uint32_t computeCRC32(const std::vector<uint8_t> &data);
uint32_t computeCRC32FromFile(const std::string &filePath);

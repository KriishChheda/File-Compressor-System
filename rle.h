#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Run-Length Encoding compression
// Format: [count][byte] pairs, where count is 1-255
// For non-repeating sequences, uses a special escape mechanism

std::vector<uint8_t> rleCompress(const std::vector<uint8_t> &data);
std::vector<uint8_t> rleDecompress(const std::vector<uint8_t> &data);

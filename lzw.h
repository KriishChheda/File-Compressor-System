#pragma once
#include <cstdint>
#include <string>
#include <vector>

// LZW (Lempel-Ziv-Welch) compression
// Dictionary-based compression that learns patterns in the data.
// Uses 12-bit codes (max dictionary size 4096).

std::vector<uint8_t> lzwCompress(const std::vector<uint8_t> &data);
std::vector<uint8_t> lzwDecompress(const std::vector<uint8_t> &data);

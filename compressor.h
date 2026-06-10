#pragma once
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────
// Supported compression algorithms
// ─────────────────────────────────────────────
enum class Algorithm { HUFFMAN = 0, RLE = 1, LZW = 2, AUTO = 99 };

// ─────────────────────────────────────────────
// Result of a compression/decompression operation
// ─────────────────────────────────────────────
struct CompressionResult {
  std::string algorithmName;
  Algorithm algorithm;
  size_t originalSize;
  size_t compressedSize;
  double compressionRatio; // percentage saved
  double timeMs;           // time taken in milliseconds
  uint32_t checksum;       // CRC32 of original data
  bool success;
  std::string errorMessage;
};

// ─────────────────────────────────────────────
// Unified file format header
// ─────────────────────────────────────────────
// Magic: "FCMP" (4 bytes)
// Version: 1 (1 byte)
// Algorithm ID (1 byte)
// CRC32 of original data (4 bytes)
// Original file size (8 bytes)
// Compressed data follows...

struct FileHeader {
  char magic[4] = {'F', 'C', 'M', 'P'};
  uint8_t version = 1;
  uint8_t algorithmId;
  uint32_t crc32;
  uint64_t originalSize;
};

// ─────────────────────────────────────────────
// Core API
// ─────────────────────────────────────────────

// Compress a file using the specified algorithm (AUTO = try all, pick best)
CompressionResult compressFile(const std::string &inputPath,
                               const std::string &outputPath,
                               Algorithm algo = Algorithm::AUTO);

// Decompress a .fcmp file (auto-detects algorithm from header)
CompressionResult decompressFile(const std::string &inputPath,
                                 const std::string &outputPath);

// Compare all algorithms on a file (returns results for each)
std::vector<CompressionResult> compareAlgorithms(const std::string &inputPath);

// Get algorithm name as string
std::string algorithmToString(Algorithm algo);

// Parse algorithm from string (for CLI)
Algorithm stringToAlgorithm(const std::string &str);

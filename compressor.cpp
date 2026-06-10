#include "compressor.h"
#include "checksum.h"
#include "huffman.h"
#include "lzw.h"
#include "rle.h"
#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────

std::string algorithmToString(Algorithm algo) {
  switch (algo) {
  case Algorithm::HUFFMAN:
    return "Huffman";
  case Algorithm::RLE:
    return "RLE";
  case Algorithm::LZW:
    return "LZW";
  case Algorithm::AUTO:
    return "Auto";
  default:
    return "Unknown";
  }
}

Algorithm stringToAlgorithm(const std::string &str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower == "huffman")
    return Algorithm::HUFFMAN;
  if (lower == "rle")
    return Algorithm::RLE;
  if (lower == "lzw")
    return Algorithm::LZW;
  if (lower == "auto")
    return Algorithm::AUTO;
  throw std::runtime_error("Unknown algorithm: " + str);
}

// Read entire file into a byte vector
static std::vector<uint8_t> readFileBytes(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    throw std::runtime_error("Cannot open file: " + path);

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  if (!file.read(reinterpret_cast<char *>(data.data()), size))
    throw std::runtime_error("Failed to read file: " + path);

  return data;
}

// Write byte vector to file
static void writeFileBytes(const std::string &path,
                           const std::vector<uint8_t> &data) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("Cannot create file: " + path);
  file.write(reinterpret_cast<const char *>(data.data()), data.size());
}

// ─────────────────────────────────────────────
// HUFFMAN COMPRESS/DECOMPRESS (byte-vector interface)
// ─────────────────────────────────────────────
// Wraps the existing Huffman tree-based implementation to work with byte
// vectors

static std::vector<uint8_t>
huffmanCompressBytes(const std::vector<uint8_t> &data) {
  // Build frequency map from data
  std::unordered_map<char, int> freqMap;
  for (uint8_t byte : data) {
    freqMap[static_cast<char>(byte)]++;
  }

  // Build tree and generate codes
  HuffmanNode *root = buildHuffmanTree(freqMap);
  std::unordered_map<char, std::string> codes;
  generateCodes(root, "", codes);

  // Sort frequency map for deterministic header
  std::vector<std::pair<char, int>> sortedFreq(freqMap.begin(), freqMap.end());
  std::sort(sortedFreq.begin(), sortedFreq.end());

  // Build output
  std::vector<uint8_t> output;

  // Write frequency table: [uniqueChars(4)] + [char(1) + freq(4)] * N
  int uniqueChars = static_cast<int>(sortedFreq.size());
  output.insert(output.end(), reinterpret_cast<uint8_t *>(&uniqueChars),
                reinterpret_cast<uint8_t *>(&uniqueChars) + sizeof(int));
  for (auto &pair : sortedFreq) {
    output.push_back(static_cast<uint8_t>(pair.first));
    output.insert(output.end(), reinterpret_cast<uint8_t *>(&pair.second),
                  reinterpret_cast<uint8_t *>(&pair.second) + sizeof(int));
  }

  // Build bit string
  std::string bitString;
  for (uint8_t byte : data) {
    bitString += codes[static_cast<char>(byte)];
  }

  // Pad to multiple of 8
  int padding = (8 - static_cast<int>(bitString.size() % 8)) % 8;
  bitString += std::string(padding, '0');

  // Write padding amount
  output.insert(output.end(), reinterpret_cast<uint8_t *>(&padding),
                reinterpret_cast<uint8_t *>(&padding) + sizeof(int));

  // Write encoded bytes
  for (size_t i = 0; i < bitString.size(); i += 8) {
    std::bitset<8> byte(bitString.substr(i, 8));
    output.push_back(static_cast<uint8_t>(byte.to_ulong()));
  }

  deleteTree(root);
  return output;
}

static std::vector<uint8_t>
huffmanDecompressBytes(const std::vector<uint8_t> &data) {
  size_t pos = 0;

  // Read frequency table
  if (data.size() < sizeof(int))
    throw std::runtime_error("Huffman: corrupt data");
  int uniqueChars;
  std::memcpy(&uniqueChars, &data[pos], sizeof(int));
  pos += sizeof(int);

  std::unordered_map<char, int> freqMap;
  for (int i = 0; i < uniqueChars; i++) {
    if (pos + 1 + sizeof(int) > data.size())
      throw std::runtime_error("Huffman: corrupt frequency table");
    char c = static_cast<char>(data[pos++]);
    int freq;
    std::memcpy(&freq, &data[pos], sizeof(int));
    pos += sizeof(int);
    freqMap[c] = freq;
  }

  // Rebuild tree
  HuffmanNode *root = buildHuffmanTree(freqMap);

  // Read padding
  if (pos + sizeof(int) > data.size())
    throw std::runtime_error("Huffman: missing padding info");
  int padding;
  std::memcpy(&padding, &data[pos], sizeof(int));
  pos += sizeof(int);

  // Convert remaining bytes to bit string
  std::string bitString;
  for (size_t i = pos; i < data.size(); i++) {
    std::bitset<8> bits(data[i]);
    bitString += bits.to_string();
  }

  // Remove padding
  if (padding > 0 && static_cast<size_t>(padding) <= bitString.size())
    bitString = bitString.substr(0, bitString.size() - padding);

  // Decode using tree
  std::vector<uint8_t> output;
  HuffmanNode *current = root;
  for (char bit : bitString) {
    current = (bit == '0') ? current->left : current->right;
    if (!current->left && !current->right) {
      output.push_back(static_cast<uint8_t>(current->ch));
      current = root;
    }
  }

  deleteTree(root);
  return output;
}

// ─────────────────────────────────────────────
// COMPRESS (single algorithm)
// ─────────────────────────────────────────────
static CompressionResult compressWithAlgorithm(const std::vector<uint8_t> &data,
                                               Algorithm algo,
                                               uint32_t crc) {
  CompressionResult result;
  result.algorithm = algo;
  result.algorithmName = algorithmToString(algo);
  result.originalSize = data.size();
  result.checksum = crc;
  result.success = true;

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<uint8_t> compressed;
  try {
    switch (algo) {
    case Algorithm::HUFFMAN:
      compressed = huffmanCompressBytes(data);
      break;
    case Algorithm::RLE:
      compressed = rleCompress(data);
      break;
    case Algorithm::LZW:
      compressed = lzwCompress(data);
      break;
    default:
      throw std::runtime_error("Invalid algorithm for direct compression");
    }
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = e.what();
    return result;
  }

  auto end = std::chrono::high_resolution_clock::now();
  result.timeMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  // Total output size = header (18 bytes) + compressed data
  result.compressedSize = sizeof(FileHeader) + compressed.size();
  result.compressionRatio =
      100.0 * (1.0 - static_cast<double>(result.compressedSize) /
                          result.originalSize);

  return result;
}

// ─────────────────────────────────────────────
// COMPRESS FILE (public API)
// ─────────────────────────────────────────────
CompressionResult compressFile(const std::string &inputPath,
                               const std::string &outputPath, Algorithm algo) {
  // Read input file
  std::vector<uint8_t> data = readFileBytes(inputPath);
  if (data.empty())
    throw std::runtime_error("Input file is empty.");

  // Compute CRC32 for integrity
  uint32_t crc = computeCRC32(data);

  // If AUTO, try all algorithms and pick the best
  if (algo == Algorithm::AUTO) {
    std::vector<Algorithm> algorithms = {Algorithm::HUFFMAN, Algorithm::RLE,
                                         Algorithm::LZW};
    CompressionResult bestResult;
    bestResult.compressedSize = SIZE_MAX;

    for (Algorithm a : algorithms) {
      CompressionResult r = compressWithAlgorithm(data, a, crc);
      if (r.success && r.compressedSize < bestResult.compressedSize) {
        bestResult = r;
      }
    }

    if (bestResult.compressedSize == SIZE_MAX)
      throw std::runtime_error("All compression algorithms failed.");

    algo = bestResult.algorithm;
    // Fall through to actually write the file with the best algorithm
  }

  // Compress with chosen algorithm
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<uint8_t> compressed;
  switch (algo) {
  case Algorithm::HUFFMAN:
    compressed = huffmanCompressBytes(data);
    break;
  case Algorithm::RLE:
    compressed = rleCompress(data);
    break;
  case Algorithm::LZW:
    compressed = lzwCompress(data);
    break;
  default:
    throw std::runtime_error("Invalid algorithm");
  }
  auto end = std::chrono::high_resolution_clock::now();

  // Build header
  FileHeader header;
  header.algorithmId = static_cast<uint8_t>(algo);
  header.crc32 = crc;
  header.originalSize = data.size();

  // Write output file: header + compressed data
  std::ofstream outFile(outputPath, std::ios::binary);
  if (!outFile.is_open())
    throw std::runtime_error("Cannot create output file: " + outputPath);

  outFile.write(reinterpret_cast<const char *>(&header), sizeof(header));
  outFile.write(reinterpret_cast<const char *>(compressed.data()),
                compressed.size());
  outFile.close();

  // Build result
  CompressionResult result;
  result.algorithm = algo;
  result.algorithmName = algorithmToString(algo);
  result.originalSize = data.size();
  result.compressedSize = sizeof(header) + compressed.size();
  result.compressionRatio =
      100.0 *
      (1.0 - static_cast<double>(result.compressedSize) / result.originalSize);
  result.timeMs =
      std::chrono::duration<double, std::milli>(end - start).count();
  result.checksum = crc;
  result.success = true;

  return result;
}

// ─────────────────────────────────────────────
// DECOMPRESS FILE (public API)
// ─────────────────────────────────────────────
CompressionResult decompressFile(const std::string &inputPath,
                                 const std::string &outputPath) {
  std::ifstream inFile(inputPath, std::ios::binary);
  if (!inFile.is_open())
    throw std::runtime_error("Cannot open file: " + inputPath);

  // Read header
  FileHeader header;
  inFile.read(reinterpret_cast<char *>(&header), sizeof(header));

  // Validate magic bytes
  if (std::strncmp(header.magic, "FCMP", 4) != 0)
    throw std::runtime_error(
        "Invalid file format. Not a .fcmp compressed file.");

  if (header.version != 1)
    throw std::runtime_error("Unsupported file format version.");

  Algorithm algo = static_cast<Algorithm>(header.algorithmId);

  // Read compressed data
  std::vector<uint8_t> compressed(
      (std::istreambuf_iterator<char>(inFile)),
      std::istreambuf_iterator<char>());
  inFile.close();

  // Decompress
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<uint8_t> decompressed;
  switch (algo) {
  case Algorithm::HUFFMAN:
    decompressed = huffmanDecompressBytes(compressed);
    break;
  case Algorithm::RLE:
    decompressed = rleDecompress(compressed);
    break;
  case Algorithm::LZW:
    decompressed = lzwDecompress(compressed);
    break;
  default:
    throw std::runtime_error("Unknown algorithm ID in file header.");
  }
  auto end = std::chrono::high_resolution_clock::now();

  // Verify CRC32 integrity
  uint32_t computedCRC = computeCRC32(decompressed);
  if (computedCRC != header.crc32) {
    throw std::runtime_error(
        "CRC32 checksum mismatch! File may be corrupted.");
  }

  // Verify size
  if (decompressed.size() != header.originalSize) {
    throw std::runtime_error(
        "Size mismatch! Expected " + std::to_string(header.originalSize) +
        " bytes, got " + std::to_string(decompressed.size()));
  }

  // Write output
  writeFileBytes(outputPath, decompressed);

  // Build result
  CompressionResult result;
  result.algorithm = algo;
  result.algorithmName = algorithmToString(algo);
  result.originalSize = header.originalSize;
  result.compressedSize = sizeof(header) + compressed.size();
  result.compressionRatio =
      100.0 *
      (1.0 - static_cast<double>(result.compressedSize) / result.originalSize);
  result.timeMs =
      std::chrono::duration<double, std::milli>(end - start).count();
  result.checksum = header.crc32;
  result.success = true;

  return result;
}

// ─────────────────────────────────────────────
// COMPARE ALGORITHMS (public API)
// ─────────────────────────────────────────────
std::vector<CompressionResult>
compareAlgorithms(const std::string &inputPath) {
  std::vector<uint8_t> data = readFileBytes(inputPath);
  if (data.empty())
    throw std::runtime_error("Input file is empty.");

  uint32_t crc = computeCRC32(data);

  std::vector<Algorithm> algorithms = {Algorithm::HUFFMAN, Algorithm::RLE,
                                       Algorithm::LZW};
  std::vector<CompressionResult> results;

  for (Algorithm algo : algorithms) {
    results.push_back(compressWithAlgorithm(data, algo, crc));
  }

  return results;
}

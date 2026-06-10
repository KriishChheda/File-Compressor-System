#include "lzw.h"
#include <stdexcept>
#include <unordered_map>

// ─────────────────────────────────────────────
// LZW COMPRESS
// ─────────────────────────────────────────────
// Uses variable-width codes starting at 9 bits, growing up to 16 bits.
// Dictionary starts with 256 single-byte entries.
// Codes are packed into bytes MSB-first.

static const int LZW_INITIAL_BITS = 9;
static const int LZW_MAX_BITS = 16;
static const int LZW_MAX_TABLE_SIZE = (1 << LZW_MAX_BITS);

// Helper: write a code of 'bits' width into the output byte stream
static void writeBits(std::vector<uint8_t> &output, uint32_t code, int bits,
                      int &bitBuffer, int &bitCount) {
  bitBuffer = (bitBuffer << bits) | (code & ((1 << bits) - 1));
  bitCount += bits;
  while (bitCount >= 8) {
    bitCount -= 8;
    output.push_back(static_cast<uint8_t>((bitBuffer >> bitCount) & 0xFF));
  }
}

// Helper: read a code of 'bits' width from the input byte stream
static uint32_t readBits(const std::vector<uint8_t> &input, size_t &bytePos,
                         int bits, int &bitBuffer, int &bitCount) {
  while (bitCount < bits) {
    if (bytePos >= input.size())
      throw std::runtime_error("LZW: unexpected end of compressed data");
    bitBuffer = (bitBuffer << 8) | input[bytePos++];
    bitCount += 8;
  }
  bitCount -= bits;
  return (bitBuffer >> bitCount) & ((1 << bits) - 1);
}

std::vector<uint8_t> lzwCompress(const std::vector<uint8_t> &data) {
  if (data.empty())
    return {};

  // Initialize dictionary with single-byte entries
  std::unordered_map<std::string, uint32_t> dictionary;
  for (int i = 0; i < 256; i++) {
    dictionary[std::string(1, static_cast<char>(i))] = i;
  }

  uint32_t nextCode = 256;
  int currentBits = LZW_INITIAL_BITS;

  std::vector<uint8_t> output;
  int bitBuffer = 0;
  int bitCount = 0;

  std::string current(1, static_cast<char>(data[0]));

  for (size_t i = 1; i < data.size(); i++) {
    std::string next = current + static_cast<char>(data[i]);

    if (dictionary.count(next)) {
      current = next;
    } else {
      // Output the code for 'current'
      writeBits(output, dictionary[current], currentBits, bitBuffer, bitCount);

      // Add new entry to dictionary
      if (nextCode < LZW_MAX_TABLE_SIZE) {
        dictionary[next] = nextCode++;
        // Increase bit width when needed
        if (nextCode > static_cast<uint32_t>((1 << currentBits)) &&
            currentBits < LZW_MAX_BITS) {
          currentBits++;
        }
      }

      current = std::string(1, static_cast<char>(data[i]));
    }
  }

  // Output the last code
  writeBits(output, dictionary[current], currentBits, bitBuffer, bitCount);

  // Flush remaining bits
  if (bitCount > 0) {
    bitBuffer <<= (8 - bitCount);
    output.push_back(static_cast<uint8_t>(bitBuffer & 0xFF));
  }

  // Prepend metadata: original size (8 bytes) + final bitCount for proper
  // decoding
  std::vector<uint8_t> result;
  uint64_t originalSize = data.size();
  for (int i = 7; i >= 0; i--) {
    result.push_back(static_cast<uint8_t>((originalSize >> (i * 8)) & 0xFF));
  }
  // Store the number of valid bits in the last byte (0 means all 8 are valid)
  uint8_t trailingBits = (bitCount > 0) ? static_cast<uint8_t>(bitCount) : 0;
  result.push_back(trailingBits);

  result.insert(result.end(), output.begin(), output.end());
  return result;
}

// ─────────────────────────────────────────────
// LZW DECOMPRESS
// ─────────────────────────────────────────────
std::vector<uint8_t> lzwDecompress(const std::vector<uint8_t> &data) {
  if (data.size() < 9)
    return {};

  // Read metadata
  uint64_t originalSize = 0;
  for (int i = 0; i < 8; i++) {
    originalSize = (originalSize << 8) | data[i];
  }
  // uint8_t trailingBits = data[8]; // reserved for future use

  // Initialize dictionary
  std::vector<std::string> dictionary;
  dictionary.reserve(LZW_MAX_TABLE_SIZE);
  for (int i = 0; i < 256; i++) {
    dictionary.push_back(std::string(1, static_cast<char>(i)));
  }

  int currentBits = LZW_INITIAL_BITS;
  size_t bytePos = 9; // skip metadata
  int bitBuffer = 0;
  int bitCount = 0;

  std::vector<uint8_t> output;
  output.reserve(originalSize);

  // Read first code
  uint32_t oldCode = readBits(data, bytePos, currentBits, bitBuffer, bitCount);
  if (oldCode >= dictionary.size())
    throw std::runtime_error("LZW: invalid first code");

  std::string current = dictionary[oldCode];
  for (char c : current)
    output.push_back(static_cast<uint8_t>(c));

  while (output.size() < originalSize) {
    uint32_t newCode;
    try {
      newCode = readBits(data, bytePos, currentBits, bitBuffer, bitCount);
    } catch (...) {
      break; // end of stream
    }

    std::string entry;
    if (newCode < dictionary.size()) {
      entry = dictionary[newCode];
    } else if (newCode == dictionary.size()) {
      // Special case: code not yet in dictionary
      entry = dictionary[oldCode] + dictionary[oldCode][0];
    } else {
      throw std::runtime_error("LZW: invalid code during decompression");
    }

    for (char c : entry) {
      if (output.size() >= originalSize)
        break;
      output.push_back(static_cast<uint8_t>(c));
    }

    // Add new dictionary entry
    if (dictionary.size() < LZW_MAX_TABLE_SIZE) {
      dictionary.push_back(dictionary[oldCode] + entry[0]);
      // Increase bit width when needed
      if (dictionary.size() + 1 > static_cast<size_t>((1 << currentBits)) &&
          currentBits < LZW_MAX_BITS) {
        currentBits++;
      }
    }

    oldCode = newCode;
  }

  output.resize(originalSize);
  return output;
}

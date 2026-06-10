#include "rle.h"
#include <stdexcept>

// ─────────────────────────────────────────────
// RLE COMPRESS
// ─────────────────────────────────────────────
// Uses a simple byte-pair scheme:
//   - If a byte repeats >= 2 times: write [count][byte]
//   - Counts stored as uint8_t (max 255 consecutive repeats)
//   - To distinguish runs from literals, we use a marker byte (0xFF).
//     Marker + count + byte = a run of 'count' copies of 'byte'
//     Any byte that is NOT preceded by a marker is a literal.
//     If the literal itself is 0xFF, we escape it as 0xFF 0x00.

static const uint8_t RLE_MARKER = 0xFF;

std::vector<uint8_t> rleCompress(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> result;
  result.reserve(data.size());

  size_t i = 0;
  while (i < data.size()) {
    // Count consecutive identical bytes
    size_t runStart = i;
    uint8_t currentByte = data[i];
    while (i < data.size() && data[i] == currentByte && (i - runStart) < 255) {
      i++;
    }
    size_t runLength = i - runStart;

    if (runLength >= 3) {
      // Worth encoding as a run: MARKER + count + byte
      result.push_back(RLE_MARKER);
      result.push_back(static_cast<uint8_t>(runLength));
      result.push_back(currentByte);
    } else {
      // Write as literals, escaping any 0xFF bytes
      for (size_t j = 0; j < runLength; j++) {
        if (currentByte == RLE_MARKER) {
          result.push_back(RLE_MARKER);
          result.push_back(0x00); // escape: 0xFF 0x00 means literal 0xFF
        } else {
          result.push_back(currentByte);
        }
      }
    }
  }

  return result;
}

// ─────────────────────────────────────────────
// RLE DECOMPRESS
// ─────────────────────────────────────────────
std::vector<uint8_t> rleDecompress(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> result;
  result.reserve(data.size() * 2);

  size_t i = 0;
  while (i < data.size()) {
    if (data[i] == RLE_MARKER) {
      if (i + 1 >= data.size())
        throw std::runtime_error("RLE: unexpected end of data after marker");

      if (data[i + 1] == 0x00) {
        // Escaped literal 0xFF
        result.push_back(RLE_MARKER);
        i += 2;
      } else {
        // Run: MARKER + count + byte
        if (i + 2 >= data.size())
          throw std::runtime_error("RLE: unexpected end of data in run");
        uint8_t count = data[i + 1];
        uint8_t byte = data[i + 2];
        for (uint8_t j = 0; j < count; j++) {
          result.push_back(byte);
        }
        i += 3;
      }
    } else {
      // Literal byte
      result.push_back(data[i]);
      i++;
    }
  }

  return result;
}

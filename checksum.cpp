#include "checksum.h"
#include <fstream>
#include <stdexcept>

// ─────────────────────────────────────────────
// CRC32 LOOKUP TABLE (precomputed)
// ─────────────────────────────────────────────
static uint32_t crcTable[256];
static bool crcTableBuilt = false;

static void buildCRCTable() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320; // CRC32 polynomial
      else
        crc >>= 1;
    }
    crcTable[i] = crc;
  }
  crcTableBuilt = true;
}

// ─────────────────────────────────────────────
// CRC32 FROM DATA
// ─────────────────────────────────────────────
uint32_t computeCRC32(const std::vector<uint8_t> &data) {
  if (!crcTableBuilt)
    buildCRCTable();

  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    crc = (crc >> 8) ^ crcTable[(crc ^ byte) & 0xFF];
  }
  return crc ^ 0xFFFFFFFF;
}

// ─────────────────────────────────────────────
// CRC32 FROM FILE
// ─────────────────────────────────────────────
uint32_t computeCRC32FromFile(const std::string &filePath) {
  if (!crcTableBuilt)
    buildCRCTable();

  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("Cannot open file for checksum: " + filePath);

  uint32_t crc = 0xFFFFFFFF;
  char buffer[8192];
  while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
    for (std::streamsize i = 0; i < file.gcount(); i++) {
      crc =
          (crc >> 8) ^ crcTable[(crc ^ static_cast<uint8_t>(buffer[i])) & 0xFF];
    }
  }
  return crc ^ 0xFFFFFFFF;
}

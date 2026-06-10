#include "archive.h"
#include "checksum.h"
#include "compressor.h"
#include "lzw.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

// Archive format (.fcarch):
// [4 bytes] Magic: "FCAR"
// [4 bytes] Number of files
// For each file:
//   [4 bytes] Relative path length
//   [N bytes] Relative path string
//   [8 bytes] Original file size
//   [8 bytes] Compressed data size
//   [4 bytes] CRC32 of original
//   [M bytes] Compressed data (using LZW)

static const char ARCHIVE_MAGIC[4] = {'F', 'C', 'A', 'R'};

void archiveDirectory(const std::string &dirPath,
                      const std::string &outputPath) {
  if (!fs::is_directory(dirPath))
    throw std::runtime_error("Not a directory: " + dirPath);

  // Collect all files recursively
  std::vector<fs::path> files;
  for (auto &entry : fs::recursive_directory_iterator(dirPath)) {
    if (entry.is_regular_file())
      files.push_back(entry.path());
  }

  if (files.empty())
    throw std::runtime_error("Directory is empty: " + dirPath);

  std::ofstream out(outputPath, std::ios::binary);
  if (!out.is_open())
    throw std::runtime_error("Cannot create archive: " + outputPath);

  // Write magic
  out.write(ARCHIVE_MAGIC, 4);

  // Write file count
  uint32_t fileCount = static_cast<uint32_t>(files.size());
  out.write(reinterpret_cast<const char *>(&fileCount), 4);

  fs::path baseDir = fs::path(dirPath);
  size_t totalOriginal = 0, totalCompressed = 0;

  for (auto &filePath : files) {
    // Get relative path
    std::string relPath = fs::relative(filePath, baseDir).string();

    // Read file data
    std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
      std::cerr << "Warning: skipping " << filePath << "\n";
      continue;
    }
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);
    std::vector<uint8_t> data(fileSize);
    inFile.read(reinterpret_cast<char *>(data.data()), fileSize);
    inFile.close();

    // Compute CRC32
    uint32_t crc = computeCRC32(data);

    // Compress with LZW
    std::vector<uint8_t> compressed = lzwCompress(data);

    // Write entry
    uint32_t pathLen = static_cast<uint32_t>(relPath.size());
    out.write(reinterpret_cast<const char *>(&pathLen), 4);
    out.write(relPath.c_str(), pathLen);

    uint64_t origSize = fileSize;
    uint64_t compSize = compressed.size();
    out.write(reinterpret_cast<const char *>(&origSize), 8);
    out.write(reinterpret_cast<const char *>(&compSize), 8);
    out.write(reinterpret_cast<const char *>(&crc), 4);
    out.write(reinterpret_cast<const char *>(compressed.data()),
              compressed.size());

    totalOriginal += fileSize;
    totalCompressed += compressed.size();

    std::cout << "  Added: " << relPath << " (" << fileSize << " -> "
              << compressed.size() << " bytes)\n";
  }

  out.close();

  double ratio =
      100.0 * (1.0 - static_cast<double>(totalCompressed) / totalOriginal);
  std::cout << "\nArchive created: " << outputPath << "\n";
  std::cout << "Files:           " << fileCount << "\n";
  std::cout << "Total original:  " << totalOriginal << " bytes\n";
  std::cout << "Total compressed:" << totalCompressed << " bytes\n";
  std::cout << "Space saved:     " << ratio << "%\n";
}

void extractArchive(const std::string &archivePath,
                    const std::string &outputDir) {
  std::ifstream in(archivePath, std::ios::binary);
  if (!in.is_open())
    throw std::runtime_error("Cannot open archive: " + archivePath);

  // Read and verify magic
  char magic[4];
  in.read(magic, 4);
  if (std::strncmp(magic, ARCHIVE_MAGIC, 4) != 0)
    throw std::runtime_error("Invalid archive format.");

  // Read file count
  uint32_t fileCount;
  in.read(reinterpret_cast<char *>(&fileCount), 4);

  // Create output directory
  fs::create_directories(outputDir);

  for (uint32_t i = 0; i < fileCount; i++) {
    // Read path
    uint32_t pathLen;
    in.read(reinterpret_cast<char *>(&pathLen), 4);
    std::string relPath(pathLen, '\0');
    in.read(&relPath[0], pathLen);

    // Read sizes and CRC
    uint64_t origSize, compSize;
    uint32_t crc;
    in.read(reinterpret_cast<char *>(&origSize), 8);
    in.read(reinterpret_cast<char *>(&compSize), 8);
    in.read(reinterpret_cast<char *>(&crc), 4);

    // Read compressed data
    std::vector<uint8_t> compressed(compSize);
    in.read(reinterpret_cast<char *>(compressed.data()), compSize);

    // Decompress
    std::vector<uint8_t> decompressed = lzwDecompress(compressed);

    // Verify integrity
    uint32_t computedCRC = computeCRC32(decompressed);
    if (computedCRC != crc) {
      std::cerr << "WARNING: CRC mismatch for " << relPath << "!\n";
    }

    // Write file
    fs::path outPath = fs::path(outputDir) / relPath;
    fs::create_directories(outPath.parent_path());
    std::ofstream outFile(outPath, std::ios::binary);
    outFile.write(reinterpret_cast<const char *>(decompressed.data()),
                  decompressed.size());
    outFile.close();

    std::cout << "  Extracted: " << relPath << " (" << decompressed.size()
              << " bytes)\n";
  }

  std::cout << "\nExtracted " << fileCount << " files to " << outputDir
            << "\n";
}

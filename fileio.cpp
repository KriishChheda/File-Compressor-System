#include "fileio.h"
#include "huffman.h"
#include <algorithm>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

// ─────────────────────────────────────────────
// ENCODE
// ─────────────────────────────────────────────
void encodeFile(const std::string &inputPath, const std::string &outputPath) {
  // 1. Build frequency map
  auto freqMap = buildFrequencyMap(inputPath);
  if (freqMap.empty())
    throw std::runtime_error("Input file is empty.");

  // 2. Build Huffman tree + generate codes
  HuffmanNode *root = buildHuffmanTree(freqMap);
  std::unordered_map<char, std::string> codes;
  generateCodes(root, "", codes);

  // 3. Open files
  std::ifstream inFile(inputPath, std::ios::binary);
  std::ofstream outFile(outputPath, std::ios::binary);
  if (!inFile.is_open() || !outFile.is_open())
    throw std::runtime_error("Cannot open files for encoding.");

  // 4. Write header: store frequency table in SORTED order
  // Sorting guarantees both encoder and decoder build identical trees
  std::vector<std::pair<char, int>> sortedFreq(freqMap.begin(), freqMap.end());
  std::sort(sortedFreq.begin(), sortedFreq.end());

  int uniqueChars = sortedFreq.size();
  outFile.write(reinterpret_cast<char *>(&uniqueChars), sizeof(uniqueChars));
  for (auto &pair : sortedFreq) {
    outFile.write(&pair.first, sizeof(char));
    outFile.write(reinterpret_cast<const char *>(&pair.second), sizeof(int));
  }

  // 5. Build the full encoded bit-string from the input file
  std::string bitString = "";
  char ch;
  while (inFile.get(ch)) {
    bitString += codes[ch];
  }

  // 6. Pad the bit-string to a multiple of 8
  int padding = (8 - (int)(bitString.size() % 8)) % 8;
  bitString += std::string(padding, '0');

  // Write padding amount so decoder knows how many bits to ignore at the end
  outFile.write(reinterpret_cast<char *>(&padding), sizeof(padding));

  // 7. Write encoded bits as actual bytes
  for (size_t i = 0; i < bitString.size(); i += 8) {
    std::bitset<8> byte(bitString.substr(i, 8));
    char byteChar = static_cast<char>(byte.to_ulong());
    outFile.write(&byteChar, sizeof(char));
  }

  inFile.close();
  outFile.close();
  deleteTree(root);

  // 8. Print compression stats (after closing files so sizes are accurate)
  auto originalSize = std::filesystem::file_size(inputPath);
  auto compressedSize = std::filesystem::file_size(outputPath);
  double ratio = 100.0 * (1.0 - (double)compressedSize / originalSize);

  std::cout << "Original size:    " << originalSize << " bytes\n";
  std::cout << "Compressed size:  " << compressedSize << " bytes\n";
  std::cout << "Space saved:      " << ratio << "%\n";
}

// ─────────────────────────────────────────────
// DECODE
// ─────────────────────────────────────────────
void decodeFile(const std::string &inputPath, const std::string &outputPath) {
  std::ifstream inFile(inputPath, std::ios::binary);
  std::ofstream outFile(outputPath, std::ios::binary);
  if (!inFile.is_open() || !outFile.is_open())
    throw std::runtime_error("Cannot open files for decoding.");

  // 1. Read header: rebuild frequency map in the same sorted order
  int uniqueChars = 0;
  inFile.read(reinterpret_cast<char *>(&uniqueChars), sizeof(uniqueChars));

  std::unordered_map<char, int> freqMap;
  for (int i = 0; i < uniqueChars; i++) {
    char c;
    int freq;
    inFile.read(&c, sizeof(char));
    inFile.read(reinterpret_cast<char *>(&freq), sizeof(int));
    freqMap[c] = freq;
  }

  // 2. Rebuild the Huffman tree (deterministic because buildHuffmanTree uses a
  // stable min-heap)
  HuffmanNode *root = buildHuffmanTree(freqMap);

  // 3. Read padding amount
  int padding = 0;
  inFile.read(reinterpret_cast<char *>(&padding), sizeof(padding));

  // 4. Read remaining bytes and convert back to a bit-string
  std::string bitString = "";
  char byte;
  while (inFile.read(&byte, sizeof(char))) {
    std::bitset<8> bits(static_cast<unsigned char>(byte));
    bitString += bits.to_string();
  }

  // Remove padding bits from the end
  if (padding > 0)
    bitString = bitString.substr(0, bitString.size() - padding);

  // 5. Walk the Huffman tree using the bit-string to decode characters
  HuffmanNode *current = root;
  for (char bit : bitString) {
    current = (bit == '0') ? current->left : current->right;

    // Reached a leaf = found a character
    if (!current->left && !current->right) {
      outFile.write(&current->ch, sizeof(char));
      current = root;
    }
  }

  std::cout << "File decoded successfully -> " << outputPath << "\n";

  deleteTree(root);
  inFile.close();
  outFile.close();
}
#include "huffman.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

std::unordered_map<char, int> buildFrequencyMap(const std::string &filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("Cannot open file: " + filePath);
  std::unordered_map<char, int> freqMap;
  char ch;
  while (file.get(ch))
    freqMap[(unsigned char)ch]++;
  return freqMap;
}

HuffmanNode *buildHuffmanTree(const std::unordered_map<char, int> &freqMap) {
  // Sort by char to ensure deterministic insertion order
  std::vector<std::pair<char, int>> sorted(freqMap.begin(), freqMap.end());
  std::sort(sorted.begin(), sorted.end());

  std::priority_queue<HuffmanNode *, std::vector<HuffmanNode *>, Compare>
      minHeap;
  int nextId = 0;
  for (auto &p : sorted)
    minHeap.push(new HuffmanNode(p.first, p.second, nextId++));

  if (minHeap.size() == 1) {
    HuffmanNode *only = minHeap.top();
    minHeap.pop();
    HuffmanNode *root = new HuffmanNode('\0', only->freq, nextId++);
    root->left = only;
    return root;
  }

  while (minHeap.size() > 1) {
    HuffmanNode *left = minHeap.top();
    minHeap.pop();
    HuffmanNode *right = minHeap.top();
    minHeap.pop();
    HuffmanNode *merged =
        new HuffmanNode('\0', left->freq + right->freq, nextId++);
    merged->left = left;
    merged->right = right;
    minHeap.push(merged);
  }
  return minHeap.top();
}

void generateCodes(HuffmanNode *root, const std::string &code,
                   std::unordered_map<char, std::string> &codes) {
  if (!root)
    return;
  if (!root->left && !root->right) {
    codes[root->ch] = code.empty() ? "0" : code;
    return;
  }
  generateCodes(root->left, code + "0", codes);
  generateCodes(root->right, code + "1", codes);
}

void deleteTree(HuffmanNode *root) {
  if (!root)
    return;
  deleteTree(root->left);
  deleteTree(root->right);
  delete root;
}
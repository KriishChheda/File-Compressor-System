#pragma once
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

struct HuffmanNode {
  char ch;
  int freq;
  int id; // insertion order: ensures deterministic tiebreaking
  HuffmanNode *left;
  HuffmanNode *right;
  HuffmanNode(char c, int f, int i)
      : ch(c), freq(f), id(i), left(nullptr), right(nullptr) {}
};

struct Compare {
  bool operator()(HuffmanNode *a, HuffmanNode *b) {
    if (a->freq != b->freq)
      return a->freq > b->freq;
    return a->id > b->id; // earlier-inserted node wins tiebreak
  }
};

std::unordered_map<char, int> buildFrequencyMap(const std::string &filePath);
HuffmanNode *buildHuffmanTree(const std::unordered_map<char, int> &freqMap);
void generateCodes(HuffmanNode *root, const std::string &code,
                   std::unordered_map<char, std::string> &codes);
void deleteTree(HuffmanNode *root);
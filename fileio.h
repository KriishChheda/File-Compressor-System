#pragma once
#include "huffman.h"
#include <string>
#include <unordered_map>

// Encode: compress input file -> output .huf file
void encodeFile(const std::string &inputPath, const std::string &outputPath);

// Decode: decompress .huf file -> restore original file
void decodeFile(const std::string &inputPath, const std::string &outputPath);
#include "archive.h"
#include "compressor.h"
#include <iomanip>
#include <iostream>
#include <string>

// ─────────────────────────────────────────────
// TERMINAL COLORS
// ─────────────────────────────────────────────
namespace Color {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string RED = "\033[31m";
const std::string BG_BLUE = "\033[44m";
} // namespace Color

// ─────────────────────────────────────────────
// ASCII ART BANNER
// ─────────────────────────────────────────────
void printBanner() {
  std::cout << Color::CYAN << Color::BOLD;
  std::cout << R"(
  ╔═══════════════════════════════════════════════╗
  ║          FILE COMPRESSOR v2.0                 ║
  ║   Huffman · RLE · LZW · Auto-Select           ║
  ╚═══════════════════════════════════════════════╝
  )" << Color::RESET << "\n";
}

// ─────────────────────────────────────────────
// USAGE
// ─────────────────────────────────────────────
void printUsage() {
  printBanner();
  std::cout << Color::BOLD << "USAGE:" << Color::RESET << "\n\n";

  std::cout << Color::GREEN << "  Compress a file:" << Color::RESET << "\n";
  std::cout << "    ./compress encode <input> <output.fcmp> [--algo "
               "huffman|rle|lzw|auto]\n\n";

  std::cout << Color::GREEN << "  Decompress a file:" << Color::RESET << "\n";
  std::cout << "    ./compress decode <input.fcmp> <output>\n\n";

  std::cout << Color::GREEN << "  Compare all algorithms:" << Color::RESET
            << "\n";
  std::cout << "    ./compress compare <input>\n\n";

  std::cout << Color::GREEN << "  Archive a directory:" << Color::RESET << "\n";
  std::cout << "    ./compress archive <directory> <output.fcarch>\n\n";

  std::cout << Color::GREEN << "  Extract an archive:" << Color::RESET << "\n";
  std::cout << "    ./compress extract <input.fcarch> <output_directory>\n\n";

  std::cout << Color::DIM << "EXAMPLES:\n";
  std::cout << "  ./compress encode report.txt report.fcmp\n";
  std::cout << "  ./compress encode report.txt report.fcmp --algo lzw\n";
  std::cout << "  ./compress decode report.fcmp report_restored.txt\n";
  std::cout << "  ./compress compare largefile.csv\n";
  std::cout << "  ./compress archive ./my_project project.fcarch\n";
  std::cout << "  ./compress extract project.fcarch ./restored\n";
  std::cout << Color::RESET << "\n";
}

// ─────────────────────────────────────────────
// VISUAL BAR
// ─────────────────────────────────────────────
std::string makeBar(double ratio, int width = 30) {
  int filled = static_cast<int>((ratio / 100.0) * width);
  if (filled < 0)
    filled = 0;
  if (filled > width)
    filled = width;
  std::string bar = "[";
  for (int i = 0; i < width; i++) {
    if (i < filled)
      bar += "█";
    else
      bar += "░";
  }
  bar += "]";
  return bar;
}

// ─────────────────────────────────────────────
// PRINT RESULT
// ─────────────────────────────────────────────
void printResult(const CompressionResult &r) {
  std::cout << "\n";
  std::cout << Color::BOLD << "  ┌─── Compression Result "
            << "───────────────────────┐" << Color::RESET << "\n";
  std::cout << "  │  Algorithm:      " << Color::CYAN << std::setw(12)
            << r.algorithmName << Color::RESET << "              │\n";
  std::cout << "  │  Original:       " << std::setw(12) << r.originalSize
            << " bytes"
            << "        │\n";
  std::cout << "  │  Compressed:     " << std::setw(12) << r.compressedSize
            << " bytes"
            << "        │\n";

  std::string ratioColor =
      (r.compressionRatio > 0) ? Color::GREEN : Color::RED;
  std::cout << "  │  Space saved:    " << ratioColor << std::setw(11)
            << std::fixed << std::setprecision(2) << r.compressionRatio << "%"
            << Color::RESET << "              │\n";

  std::cout << "  │  Time:           " << std::setw(10) << std::fixed
            << std::setprecision(2) << r.timeMs << " ms"
            << "             │\n";

  std::cout << "  │  CRC32:          " << std::hex << std::setw(12)
            << r.checksum << std::dec << Color::RESET << "              │\n";

  double barRatio = std::max(0.0, r.compressionRatio);
  std::cout << "  │  " << makeBar(barRatio) << " │\n";

  std::cout << Color::BOLD
            << "  └─────────────────────────────────────────────┘"
            << Color::RESET << "\n\n";
}

// ─────────────────────────────────────────────
// PRINT COMPARISON TABLE
// ─────────────────────────────────────────────
void printComparison(const std::vector<CompressionResult> &results) {
  std::cout << "\n";
  std::cout << Color::BOLD << Color::CYAN
            << "  ╔═══════════════════════════════════════════════"
               "════════════════════════╗"
            << Color::RESET << "\n";
  std::cout << Color::BOLD << Color::CYAN
            << "  ║              ALGORITHM COMPARISON              "
               "                      ║"
            << Color::RESET << "\n";
  std::cout << Color::BOLD << Color::CYAN
            << "  ╠══════════╦══════════════╦══════════════╦═══════"
               "════╦═════════════════╣"
            << Color::RESET << "\n";
  std::cout << Color::BOLD
            << "  ║ Algo     ║ Original     ║ Compressed   ║ "
               "Saved     ║ Time            ║"
            << Color::RESET << "\n";
  std::cout << Color::BOLD << Color::CYAN
            << "  ╠══════════╬══════════════╬══════════════╬═══════"
               "════╬═════════════════╣"
            << Color::RESET << "\n";

  // Find best result
  size_t bestIdx = 0;
  for (size_t i = 1; i < results.size(); i++) {
    if (results[i].success &&
        results[i].compressedSize < results[bestIdx].compressedSize)
      bestIdx = i;
  }

  for (size_t i = 0; i < results.size(); i++) {
    auto &r = results[i];
    std::string marker = (i == bestIdx) ? " ★" : "  ";
    std::string color = (i == bestIdx) ? Color::GREEN : "";
    std::string reset = (i == bestIdx) ? Color::RESET : "";

    if (r.success) {
      std::cout << "  ║ " << color << std::setw(8) << std::left
                << (r.algorithmName + marker) << reset << " ║ " << std::right
                << std::setw(10) << r.originalSize << " B  ║ " << std::setw(10)
                << r.compressedSize << " B  ║ " << std::setw(8) << std::fixed
                << std::setprecision(2) << r.compressionRatio << "% ║ "
                << std::setw(12) << std::fixed << std::setprecision(2)
                << r.timeMs << " ms ║\n";
    } else {
      std::cout << "  ║ " << std::setw(8) << std::left << r.algorithmName
                << " ║ " << Color::RED << std::setw(10) << "FAILED"
                << Color::RESET << "    ║              ║           ║"
                << "                 ║\n";
    }
  }

  std::cout << Color::BOLD << Color::CYAN
            << "  ╚══════════╩══════════════╩══════════════╩═══════"
               "════╩═════════════════╝"
            << Color::RESET << "\n";

  std::cout << "\n  " << Color::GREEN << "★" << Color::RESET
            << " = Best algorithm for this file\n\n";
}

// ─────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  std::string mode = argv[1];

  try {
    // ── ENCODE ──
    if (mode == "encode") {
      if (argc < 4) {
        printUsage();
        return 1;
      }
      std::string inputFile = argv[2];
      std::string outputFile = argv[3];
      Algorithm algo = Algorithm::AUTO;

      // Parse optional --algo flag
      for (int i = 4; i < argc; i++) {
        if (std::string(argv[i]) == "--algo" && i + 1 < argc) {
          algo = stringToAlgorithm(argv[i + 1]);
          i++;
        }
      }

      printBanner();
      std::cout << "  Compressing: " << inputFile << "\n";
      if (algo == Algorithm::AUTO)
        std::cout << "  Mode: " << Color::CYAN << "Auto-select best algorithm"
                  << Color::RESET << "\n";
      else
        std::cout << "  Algorithm: " << Color::CYAN << algorithmToString(algo)
                  << Color::RESET << "\n";

      CompressionResult result = compressFile(inputFile, outputFile, algo);
      printResult(result);
      std::cout << "  " << Color::GREEN << "✓" << Color::RESET << " Saved to "
                << Color::BOLD << outputFile << Color::RESET << "\n\n";
    }

    // ── DECODE ──
    else if (mode == "decode") {
      if (argc < 4) {
        printUsage();
        return 1;
      }
      std::string inputFile = argv[2];
      std::string outputFile = argv[3];

      printBanner();
      std::cout << "  Decompressing: " << inputFile << "\n";

      CompressionResult result = decompressFile(inputFile, outputFile);

      std::cout << "\n  " << Color::GREEN << "✓" << Color::RESET
                << " Decoded using " << Color::CYAN << result.algorithmName
                << Color::RESET << "\n";
      std::cout << "  " << Color::GREEN << "✓" << Color::RESET
                << " CRC32 integrity verified\n";
      std::cout << "  " << Color::GREEN << "✓" << Color::RESET << " Saved to "
                << Color::BOLD << outputFile << Color::RESET << "\n\n";
    }

    // ── COMPARE ──
    else if (mode == "compare") {
      if (argc < 3) {
        printUsage();
        return 1;
      }
      std::string inputFile = argv[2];

      printBanner();
      std::cout << "  Analyzing: " << inputFile << "\n";

      auto results = compareAlgorithms(inputFile);
      printComparison(results);
    }

    // ── ARCHIVE ──
    else if (mode == "archive") {
      if (argc < 4) {
        printUsage();
        return 1;
      }
      std::string dirPath = argv[2];
      std::string outputFile = argv[3];

      printBanner();
      std::cout << "  Archiving directory: " << dirPath << "\n\n";
      archiveDirectory(dirPath, outputFile);
    }

    // ── EXTRACT ──
    else if (mode == "extract") {
      if (argc < 4) {
        printUsage();
        return 1;
      }
      std::string archivePath = argv[2];
      std::string outputDir = argv[3];

      printBanner();
      std::cout << "  Extracting: " << archivePath << "\n\n";
      extractArchive(archivePath, outputDir);
    }

    // ── UNKNOWN ──
    else {
      std::cerr << Color::RED << "Unknown mode: " << mode << Color::RESET
                << "\n";
      printUsage();
      return 1;
    }
  } catch (const std::exception &e) {
    std::cerr << "\n  " << Color::RED << "✗ Error: " << e.what()
              << Color::RESET << "\n\n";
    return 1;
  }

  return 0;
}
#pragma once
#include <string>

// Archive a directory into a single .fcarch file
void archiveDirectory(const std::string &dirPath,
                      const std::string &outputPath);

// Extract a .fcarch archive to a directory
void extractArchive(const std::string &archivePath,
                    const std::string &outputDir);

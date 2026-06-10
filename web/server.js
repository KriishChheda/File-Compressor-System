const express = require("express");
const multer = require("multer");
const path = require("path");
const fs = require("fs");
const { execSync } = require("child_process");

const app = express();
const PORT = 3000;

// Path to compiled C++ binary
const COMPRESS_BIN = path.join(__dirname, "..", "compress");

// Ensure uploads and output directories exist
const uploadsDir = path.join(__dirname, "uploads");
const outputDir = path.join(__dirname, "output");
fs.mkdirSync(uploadsDir, { recursive: true });
fs.mkdirSync(outputDir, { recursive: true });

// Multer configuration for file uploads
const storage = multer.diskStorage({
  destination: uploadsDir,
  filename: (req, file, cb) => {
    const uniqueName = Date.now() + "-" + file.originalname;
    cb(null, uniqueName);
  },
});
const upload = multer({ storage, limits: { fileSize: 100 * 1024 * 1024 } }); // 100MB limit

// Serve static files
app.use(express.static(path.join(__dirname, "public")));

// ─── COMPRESS ENDPOINT ───
app.post("/api/compress", upload.single("file"), (req, res) => {
  try {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const algo = req.body.algorithm || "auto";
    const inputPath = req.file.path;
    const outputName = req.file.filename + ".fcmp";
    const outputPath = path.join(outputDir, outputName);

    const cmd = `"${COMPRESS_BIN}" encode "${inputPath}" "${outputPath}" --algo ${algo}`;
    const output = execSync(cmd, { encoding: "utf-8", timeout: 30000 });

    // Parse compression stats from output
    const stats = parseStats(output);

    res.json({
      success: true,
      downloadUrl: `/api/download/${outputName}`,
      originalName: req.file.originalname,
      ...stats,
    });
  } catch (err) {
    res.status(500).json({ error: err.message || "Compression failed" });
  }
});

// ─── COMPARE ENDPOINT ───
app.post("/api/compare", upload.single("file"), (req, res) => {
  try {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const inputPath = req.file.path;
    const cmd = `"${COMPRESS_BIN}" compare "${inputPath}"`;
    const output = execSync(cmd, { encoding: "utf-8", timeout: 30000 });

    const results = parseCompareResults(output, req.file.size);

    res.json({
      success: true,
      originalName: req.file.originalname,
      originalSize: req.file.size,
      results,
    });
  } catch (err) {
    res.status(500).json({ error: err.message || "Comparison failed" });
  }
});

// ─── DECOMPRESS ENDPOINT ───
app.post("/api/decompress", upload.single("file"), (req, res) => {
  try {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const inputPath = req.file.path;
    // Strip .fcmp extension for output name
    let baseName = req.file.originalname;
    if (baseName.endsWith(".fcmp")) baseName = baseName.slice(0, -5);
    const outputName = Date.now() + "-" + baseName;
    const outputPath = path.join(outputDir, outputName);

    const cmd = `"${COMPRESS_BIN}" decode "${inputPath}" "${outputPath}"`;
    execSync(cmd, { encoding: "utf-8", timeout: 30000 });

    res.json({
      success: true,
      downloadUrl: `/api/download/${outputName}`,
      originalName: baseName,
    });
  } catch (err) {
    res.status(500).json({ error: err.message || "Decompression failed" });
  }
});

// ─── DOWNLOAD ENDPOINT ───
app.get("/api/download/:filename", (req, res) => {
  const filePath = path.join(outputDir, req.params.filename);
  if (!fs.existsSync(filePath)) {
    return res.status(404).json({ error: "File not found" });
  }
  res.download(filePath);
});

// ─── PARSE HELPERS ───
function parseStats(output) {
  const stats = {};

  // Strip ANSI codes
  const clean = output.replace(/\x1b\[[0-9;]*m/g, "");

  const algoMatch = clean.match(/Algorithm:\s*(\w+)/);
  if (algoMatch) stats.algorithm = algoMatch[1];

  const origMatch = clean.match(/Original:\s*([\d,]+)\s*bytes/);
  if (origMatch) stats.originalSize = parseInt(origMatch[1].replace(/,/g, ""));

  const compMatch = clean.match(/Compressed:\s*([\d,]+)\s*bytes/);
  if (compMatch)
    stats.compressedSize = parseInt(compMatch[1].replace(/,/g, ""));

  const savedMatch = clean.match(/Space saved:\s*([-\d.]+)%/);
  if (savedMatch) stats.spaceSaved = parseFloat(savedMatch[1]);

  const timeMatch = clean.match(/Time:\s*([\d.]+)\s*ms/);
  if (timeMatch) stats.timeMs = parseFloat(timeMatch[1]);

  const crcMatch = clean.match(/CRC32:\s*([0-9a-f]+)/i);
  if (crcMatch) stats.checksum = crcMatch[1];

  return stats;
}

function parseCompareResults(output, fileSize) {
  const clean = output.replace(/\x1b\[[0-9;]*m/g, "");
  const lines = clean.split("\n");
  const results = [];

  for (const line of lines) {
    // Match table rows like: ║ Huffman  ║     249 B  ║     364 B  ║   -46.18% ║ ...
    const match = line.match(
      /║\s*([\w★ ]+?)\s*║\s*([\d]+)\s*B\s*║\s*([\d]+)\s*B\s*║\s*([-\d.]+)%\s*║\s*([\d.]+)\s*ms\s*║/
    );
    if (match) {
      results.push({
        algorithm: match[1].replace("★", "").trim(),
        originalSize: parseInt(match[2]),
        compressedSize: parseInt(match[3]),
        spaceSaved: parseFloat(match[4]),
        timeMs: parseFloat(match[5]),
        isBest: match[1].includes("★"),
      });
    }
  }

  return results;
}

// ─── CLEANUP (on startup, remove old files) ───
function cleanupOldFiles() {
  const maxAge = 60 * 60 * 1000; // 1 hour
  [uploadsDir, outputDir].forEach((dir) => {
    if (!fs.existsSync(dir)) return;
    fs.readdirSync(dir).forEach((file) => {
      const filePath = path.join(dir, file);
      const stat = fs.statSync(filePath);
      if (Date.now() - stat.mtimeMs > maxAge) {
        fs.unlinkSync(filePath);
      }
    });
  });
}
cleanupOldFiles();

app.listen(PORT, () => {
  console.log(`\n  🗜️  File Compressor Web UI`);
  console.log(`  ─────────────────────────`);
  console.log(`  Running at: http://localhost:${PORT}`);
  console.log(`  Binary:     ${COMPRESS_BIN}\n`);
});

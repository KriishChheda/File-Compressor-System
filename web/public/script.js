// ─────────────────────────────────────────────
// TABS
// ─────────────────────────────────────────────
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
    tab.classList.add('active');
    document.getElementById(`section-${tab.dataset.tab}`).classList.add('active');
  });
});

// ─────────────────────────────────────────────
// FILE DROP ZONES
// ─────────────────────────────────────────────
function setupDropZone(dropzoneId, inputId, infoId, btnId) {
  const dropzone = document.getElementById(dropzoneId);
  const input = document.getElementById(inputId);
  const infoContainer = document.getElementById(infoId);
  const btn = document.getElementById(btnId);
  let selectedFile = null;

  dropzone.addEventListener('click', () => input.click());

  dropzone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropzone.classList.add('dragover');
  });

  dropzone.addEventListener('dragleave', () => {
    dropzone.classList.remove('dragover');
  });

  dropzone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropzone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) {
      setFile(e.dataTransfer.files[0]);
    }
  });

  input.addEventListener('change', () => {
    if (input.files.length > 0) setFile(input.files[0]);
  });

  function setFile(file) {
    selectedFile = file;
    btn.disabled = false;

    infoContainer.style.display = 'block';
    infoContainer.innerHTML = `
      <div class="file-selected">
        <span class="file-selected-icon">📄</span>
        <div class="file-selected-info">
          <div class="file-selected-name">${escapeHtml(file.name)}</div>
          <div class="file-selected-size">${formatSize(file.size)}</div>
        </div>
        <button class="file-selected-remove" onclick="clearFile_${dropzoneId}()">✕</button>
      </div>
    `;
  }

  // Expose clear function globally
  window[`clearFile_${dropzoneId}`] = function() {
    selectedFile = null;
    btn.disabled = true;
    infoContainer.style.display = 'none';
    infoContainer.innerHTML = '';
    input.value = '';
  };

  // Return getter for selected file
  return () => selectedFile;
}

const getCompressFile = setupDropZone('compress-dropzone', 'compress-file-input', 'compress-file-info', 'compress-btn');
const getDecompressFile = setupDropZone('decompress-dropzone', 'decompress-file-input', 'decompress-file-info', 'decompress-btn');
const getCompareFile = setupDropZone('compare-dropzone', 'compare-file-input', 'compare-file-info', 'compare-btn');

// ─────────────────────────────────────────────
// ALGORITHM SELECTOR
// ─────────────────────────────────────────────
let selectedAlgo = 'auto';

document.getElementById('algo-grid').addEventListener('click', (e) => {
  const option = e.target.closest('.algo-option');
  if (!option) return;

  document.querySelectorAll('.algo-option').forEach(o => o.classList.remove('selected'));
  option.classList.add('selected');
  selectedAlgo = option.dataset.algo;
});

// ─────────────────────────────────────────────
// COMPRESS
// ─────────────────────────────────────────────
document.getElementById('compress-btn').addEventListener('click', async () => {
  const file = getCompressFile();
  if (!file) return;

  showSpinner('compress-spinner');
  hideError('compress-error');
  clearResult('compress-result');

  const formData = new FormData();
  formData.append('file', file);
  formData.append('algorithm', selectedAlgo);

  try {
    const res = await fetch('/api/compress', { method: 'POST', body: formData });
    const data = await res.json();

    hideSpinner('compress-spinner');

    if (!data.success) throw new Error(data.error);

    const saved = data.spaceSaved || 0;
    const barClass = saved > 20 ? 'good' : saved > 0 ? 'ok' : 'bad';
    const barWidth = Math.max(0, Math.min(100, saved));
    const valueClass = saved > 0 ? 'positive' : 'negative';

    document.getElementById('compress-result').innerHTML = `
      <div class="result-card">
        <div class="result-header">
          <span class="result-header-icon">✅</span>
          <div>
            <h3>Compression Complete</h3>
            <span>Algorithm: ${data.algorithm || 'Auto'}</span>
          </div>
        </div>

        <div class="stats-grid">
          <div class="stat-item">
            <div class="stat-label">Original Size</div>
            <div class="stat-value">${formatSize(data.originalSize)}</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">Compressed Size</div>
            <div class="stat-value">${formatSize(data.compressedSize)}</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">Space Saved</div>
            <div class="stat-value ${valueClass}">${saved.toFixed(2)}%</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">Checksum (CRC32)</div>
            <div class="stat-value" style="font-size:16px;color:var(--text-secondary)">${data.checksum || 'N/A'}</div>
          </div>
        </div>

        <div class="progress-bar-container">
          <div class="progress-bar-label">
            <span>Compression Ratio</span>
            <span>${saved.toFixed(1)}%</span>
          </div>
          <div class="progress-bar">
            <div class="progress-bar-fill ${barClass}" style="width:${barWidth}%"></div>
          </div>
        </div>

        <a href="${data.downloadUrl}" class="btn btn-green btn-full" download>
          <span>⬇️</span> Download Compressed File
        </a>
      </div>
    `;
  } catch (err) {
    hideSpinner('compress-spinner');
    showError('compress-error', err.message);
  }
});

// ─────────────────────────────────────────────
// DECOMPRESS
// ─────────────────────────────────────────────
document.getElementById('decompress-btn').addEventListener('click', async () => {
  const file = getDecompressFile();
  if (!file) return;

  showSpinner('decompress-spinner');
  hideError('decompress-error');
  clearResult('decompress-result');

  const formData = new FormData();
  formData.append('file', file);

  try {
    const res = await fetch('/api/decompress', { method: 'POST', body: formData });
    const data = await res.json();

    hideSpinner('decompress-spinner');

    if (!data.success) throw new Error(data.error);

    document.getElementById('decompress-result').innerHTML = `
      <div class="result-card">
        <div class="result-header">
          <span class="result-header-icon">✅</span>
          <div>
            <h3>Decompression Complete</h3>
            <span>CRC32 integrity verified</span>
          </div>
        </div>
        <a href="${data.downloadUrl}" class="btn btn-green btn-full" download="${escapeHtml(data.originalName)}">
          <span>⬇️</span> Download Restored File
        </a>
      </div>
    `;
  } catch (err) {
    hideSpinner('decompress-spinner');
    showError('decompress-error', err.message);
  }
});

// ─────────────────────────────────────────────
// COMPARE
// ─────────────────────────────────────────────
document.getElementById('compare-btn').addEventListener('click', async () => {
  const file = getCompareFile();
  if (!file) return;

  showSpinner('compare-spinner');
  hideError('compare-error');
  clearResult('compare-result');

  const formData = new FormData();
  formData.append('file', file);

  try {
    const res = await fetch('/api/compare', { method: 'POST', body: formData });
    const data = await res.json();

    hideSpinner('compare-spinner');

    if (!data.success) throw new Error(data.error);

    let tableRows = '';
    data.results.forEach(r => {
      const algoClass = r.algorithm.toLowerCase();
      const savedClass = r.spaceSaved > 0 ? 'positive' : 'negative';
      const bestMark = r.isBest ? '<span class="best-star">★ Best</span>' : '';
      const rowClass = r.isBest ? 'best' : '';

      tableRows += `
        <tr class="${rowClass}">
          <td><span class="algo-badge ${algoClass}">${r.algorithm}</span>${bestMark}</td>
          <td>${formatSize(r.originalSize)}</td>
          <td>${formatSize(r.compressedSize)}</td>
          <td class="stat-value ${savedClass}" style="font-size:14px">${r.spaceSaved.toFixed(2)}%</td>
          <td>${r.timeMs.toFixed(2)} ms</td>
        </tr>
      `;
    });

    document.getElementById('compare-result').innerHTML = `
      <div class="result-card">
        <div class="result-header">
          <span class="result-header-icon">📊</span>
          <div>
            <h3>Algorithm Comparison</h3>
            <span>${escapeHtml(data.originalName)} · ${formatSize(data.originalSize)}</span>
          </div>
        </div>
        <table class="compare-table">
          <thead>
            <tr>
              <th>Algorithm</th>
              <th>Original</th>
              <th>Compressed</th>
              <th>Saved</th>
              <th>Time</th>
            </tr>
          </thead>
          <tbody>${tableRows}</tbody>
        </table>
      </div>
    `;
  } catch (err) {
    hideSpinner('compare-spinner');
    showError('compare-error', err.message);
  }
});

// ─────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────
function formatSize(bytes) {
  if (!bytes && bytes !== 0) return 'N/A';
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

function showSpinner(id) { document.getElementById(id).classList.add('active'); }
function hideSpinner(id) { document.getElementById(id).classList.remove('active'); }
function showError(id, msg) {
  const el = document.getElementById(id);
  el.textContent = '❌ ' + msg;
  el.classList.add('active');
}
function hideError(id) { document.getElementById(id).classList.remove('active'); }
function clearResult(id) { document.getElementById(id).innerHTML = ''; }

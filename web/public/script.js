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

  dropzone.addEventListener('dragover', e => {
    e.preventDefault();
    dropzone.classList.add('dragover');
  });

  dropzone.addEventListener('dragleave', () => {
    dropzone.classList.remove('dragover');
  });

  dropzone.addEventListener('drop', e => {
    e.preventDefault();
    dropzone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) setFile(e.dataTransfer.files[0]);
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
        <div class="file-selected-dot"></div>
        <div class="file-selected-info">
          <div class="file-selected-name">${escapeHtml(file.name)}</div>
          <div class="file-selected-size">${formatSize(file.size)}</div>
        </div>
        <button class="file-selected-remove" onclick="clearFile_${dropzoneId}()" aria-label="Remove file">&times;</button>
      </div>
    `;
  }

  window[`clearFile_${dropzoneId}`] = function () {
    selectedFile = null;
    btn.disabled = true;
    infoContainer.style.display = 'none';
    infoContainer.innerHTML = '';
    input.value = '';
  };

  return () => selectedFile;
}

const getCompressFile = setupDropZone('compress-dropzone', 'compress-file-input', 'compress-file-info', 'compress-btn');
const getDecompressFile = setupDropZone('decompress-dropzone', 'decompress-file-input', 'decompress-file-info', 'decompress-btn');
const getCompareFile = setupDropZone('compare-dropzone', 'compare-file-input', 'compare-file-info', 'compare-btn');

// ─────────────────────────────────────────────
// ALGORITHM SELECTOR
// ─────────────────────────────────────────────
let selectedAlgo = 'auto';

document.getElementById('algo-grid').addEventListener('click', e => {
  const option = e.target.closest('.algo-option');
  if (!option) return;
  document.querySelectorAll('.algo-option').forEach(o => o.classList.remove('selected'));
  option.classList.add('selected');
  selectedAlgo = option.dataset.algo;
});

// ─────────────────────────────────────────────
// SVG ICONS
// ─────────────────────────────────────────────
const icons = {
  check: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>`,
  download: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>`,
  trophy: `<svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2l3.09 6.26L22 9.27l-5 4.87 1.18 6.88L12 17.77l-6.18 3.25L7 14.14 2 9.27l6.91-1.01L12 2z"/></svg>`,
};

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
          <div class="result-check">${icons.check}</div>
          <div>
            <div class="result-title">Compression Complete</div>
            <div class="result-subtitle">Algorithm: ${data.algorithm || 'Auto'}</div>
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
            <div class="stat-label">CRC32 Checksum</div>
            <div class="stat-value mono" style="font-size:14px;color:var(--text-3)">${data.checksum || '—'}</div>
          </div>
        </div>

        <div class="progress-wrap">
          <div class="progress-label">
            <span>Compression ratio</span>
            <span>${saved.toFixed(1)}%</span>
          </div>
          <div class="progress-track">
            <div class="progress-fill ${barClass}" style="width:${barWidth}%"></div>
          </div>
        </div>

        <a href="${data.downloadUrl}" class="btn btn-success btn-full" download>
          ${icons.download} Download Compressed File
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
          <div class="result-check">${icons.check}</div>
          <div>
            <div class="result-title">Decompression Complete</div>
            <div class="result-subtitle">CRC32 integrity verified</div>
          </div>
        </div>
        <a href="${data.downloadUrl}" class="btn btn-success btn-full" download="${escapeHtml(data.originalName)}">
          ${icons.download} Download Restored File
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

    let rows = '';
    data.results.forEach(r => {
      const algoClass = r.algorithm.toLowerCase();
      const savedClass = r.spaceSaved > 0 ? 'positive' : 'negative';
      const bestHtml = r.isBest
        ? `<span class="best-indicator">${icons.trophy} Best</span>`
        : '';
      const rowClass = r.isBest ? 'best' : '';

      rows += `
        <tr class="${rowClass}">
          <td><span class="algo-badge ${algoClass}">${r.algorithm}</span>${bestHtml}</td>
          <td class="mono">${formatSize(r.compressedSize)}</td>
          <td class="stat-value ${savedClass}" style="font-size:13px">${r.spaceSaved.toFixed(2)}%</td>
          <td class="mono">${r.timeMs.toFixed(2)} ms</td>
        </tr>
      `;
    });

    document.getElementById('compare-result').innerHTML = `
      <div class="result-card">
        <div class="result-header">
          <div class="result-check">${icons.check}</div>
          <div>
            <div class="result-title">Algorithm Comparison</div>
            <div class="result-subtitle">${escapeHtml(data.originalName)} &middot; ${formatSize(data.originalSize)}</div>
          </div>
        </div>
        <table class="compare-table">
          <thead>
            <tr>
              <th>Algorithm</th>
              <th>Compressed</th>
              <th>Saved</th>
              <th>Time</th>
            </tr>
          </thead>
          <tbody>${rows}</tbody>
        </table>
      </div>
    `;
  } catch (err) {
    hideSpinner('compare-spinner');
    showError('compare-error', err.message);
  }
});

// ─────────────────────────────────────────────
// UTILITIES
// ─────────────────────────────────────────────
function formatSize(bytes) {
  if (bytes == null) return '—';
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function escapeHtml(str) {
  const el = document.createElement('span');
  el.textContent = str;
  return el.innerHTML;
}

function showSpinner(id) { document.getElementById(id).classList.add('active'); }
function hideSpinner(id) { document.getElementById(id).classList.remove('active'); }

function showError(id, msg) {
  const el = document.getElementById(id);
  el.textContent = msg;
  el.classList.add('active');
}

function hideError(id) { document.getElementById(id).classList.remove('active'); }
function clearResult(id) { document.getElementById(id).innerHTML = ''; }

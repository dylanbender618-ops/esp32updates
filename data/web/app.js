const state = {
  system: {},
  files: [],
  currentPage: 'dashboard',
  ws: null,
  autoSaveTimer: null,
};

const $ = (id) => document.getElementById(id);

function toast(message, kind = 'ok') {
  const wrap = $('toastWrap');
  const el = document.createElement('div');
  el.className = `toast ${kind}`;
  el.textContent = message;
  wrap.appendChild(el);
  setTimeout(() => el.remove(), 3200);
}

async function api(path, opts = {}) {
  const res = await fetch(path, {
    credentials: 'include',
    headers: { 'Content-Type': 'application/json', ...(opts.headers || {}) },
    ...opts,
  });
  if (res.status === 401) {
    showLogin();
    throw new Error('Unauthorized');
  }
  const ct = res.headers.get('content-type') || '';
  if (ct.includes('application/json')) return res.json();
  return res.text();
}

function showLogin() {
  $('desktop').classList.add('hidden');
  $('loginView').classList.remove('hidden');
}

function showDesktop() {
  $('loginView').classList.add('hidden');
  $('desktop').classList.remove('hidden');
}

function switchPage(page) {
  state.currentPage = page;
  document.querySelectorAll('.nav-btn').forEach((b) => b.classList.toggle('active', b.dataset.page === page));
  document.querySelectorAll('.page').forEach((p) => p.classList.toggle('active', p.id === page));
  api(`/api/system?page=${encodeURIComponent(page)}`).catch(() => {});
  if (state.ws?.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify({ action: 'setPage', page }));
  }
}

function renderMetrics(system) {
  const map = {
    Hostname: system.hostname,
    IP: system.ip,
    'CPU Freq': `${system.cpuFreqMHz} MHz`,
    'Free Heap': `${system.freeHeap} B`,
    'Flash Size': `${Math.round(system.flashSize / 1024 / 1024)} MB`,
    'FS Usage': `${system.fsUsed}/${system.fsTotal} B`,
    RSSI: system.rssi,
    Clients: system.clients,
    Uptime: system.uptime,
    Time: String(system.time),
  };
  $('metrics').innerHTML = Object.entries(map)
    .map(([k, v]) => `<div class="card"><h3>${k}</h3><div class="value">${v ?? '-'}</div></div>`)
    .join('');
}

async function loadSystem() {
  const data = await api('/api/system');
  state.system = data;
  renderMetrics(data);
}

function flattenEntries(entries = []) {
  const out = [];
  const walk = (nodes) => {
    nodes.forEach((node) => {
      out.push(node);
      if (node.children?.length) walk(node.children);
    });
  };
  walk(entries);
  return out;
}

function formatSize(size) {
  if (!size) return '0 B';
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${(size / 1024).toFixed(1)} KB`;
  return `${(size / (1024 * 1024)).toFixed(2)} MB`;
}

function fileRow(file) {
  const name = file.name || file.path;
  const path = file.path;
  const type = file.isDir ? 'dir' : 'file';
  return `<tr>
    <td>${name}</td>
    <td>${type}</td>
    <td>${formatSize(file.size)}</td>
    <td>
      <button onclick="tinyOpenFile('${encodeURIComponent(path)}')">Open</button>
      <button onclick="tinyDownload('${encodeURIComponent(path)}')">Download</button>
      <button onclick="tinyDelete('${encodeURIComponent(path)}')" class="danger">Delete</button>
    </td>
  </tr>`;
}

async function loadFiles() {
  const path = $('filePath').value || '/';
  const search = $('fileSearch').value.trim().toLowerCase();
  const data = await api(`/api/files?path=${encodeURIComponent(path)}`);
  const files = flattenEntries(data.entries || []);
  const filtered = search ? files.filter((f) => (f.path || '').toLowerCase().includes(search)) : files;
  filtered.sort((a, b) => (a.isDir === b.isDir ? a.path.localeCompare(b.path) : a.isDir ? -1 : 1));
  state.files = filtered;
  $('fileList').innerHTML = filtered.map(fileRow).join('') || '<tr><td colspan="4">No files</td></tr>';
}

window.tinyDelete = async (encodedPath) => {
  const path = decodeURIComponent(encodedPath);
  if (!confirm(`Delete ${path}?`)) return;
  const res = await api('/api/files', { method: 'POST', body: JSON.stringify({ action: 'delete', path }) });
  toast(res.ok ? 'Deleted' : 'Delete failed', res.ok ? 'ok' : 'err');
  loadFiles();
};

window.tinyDownload = (encodedPath) => {
  const path = decodeURIComponent(encodedPath);
  window.open(`/api/download?path=${encodeURIComponent(path)}`, '_blank');
  toast('Download started');
};

window.tinyOpenFile = async (encodedPath) => {
  const path = decodeURIComponent(encodedPath);
  $('editorPath').value = path;
  switchPage('editor');
  await window.tinyEditorLoad();
};

async function uploadFiles(files) {
  const path = $('filePath').value || '/';
  for (const file of files) {
    const form = new FormData();
    form.append('path', path);
    form.append('file', file, file.name);
    const res = await fetch('/api/upload', { method: 'POST', credentials: 'include', body: form });
    if (!res.ok) throw new Error(`Upload failed: ${file.name}`);
    toast(`Uploaded ${file.name}`);
  }
  await loadFiles();
}

function initDropzone() {
  const zone = $('dropZone');
  ['dragenter', 'dragover'].forEach((evt) => zone.addEventListener(evt, (e) => {
    e.preventDefault();
    zone.classList.add('drag');
  }));
  ['dragleave', 'drop'].forEach((evt) => zone.addEventListener(evt, (e) => {
    e.preventDefault();
    zone.classList.remove('drag');
  }));
  zone.addEventListener('drop', (e) => {
    const files = [...e.dataTransfer.files];
    if (files.length) uploadFiles(files).catch((err) => toast(err.message, 'err'));
  });
}

async function loadSettings() {
  const s = await api('/api/settings');
  $('setHostname').value = s.hostname || '';
  $('setTheme').value = s.theme || 'dark';
  $('setBootPage').value = s.autoBootPage || 'dashboard';
  $('setTerminalFg').value = s.terminalFg || '#d9fdd3';
  $('setTerminalBg').value = s.terminalBg || '#0d1117';
  $('setBrightness').value = s.oledBrightness ?? 255;
  $('setBootLogo').value = String(Boolean(s.bootLogoEnabled));
  $('setWifiSsid').value = s.wifiSsid || '';
  $('setWifiPassword').value = '';
}


async function saveSettings() {
  const payload = {
    hostname: $('setHostname').value,
    theme: $('setTheme').value,
    autoBootPage: $('setBootPage').value,
    terminalFg: $('setTerminalFg').value,
    terminalBg: $('setTerminalBg').value,
    oledBrightness: Number($('setBrightness').value),
    bootLogoEnabled: $('setBootLogo').value === 'true',
  };
  if ($('setPassword').value.trim()) payload.password = $('setPassword').value.trim();
  const res = await api('/api/settings', { method: 'POST', body: JSON.stringify(payload) });
  toast(res.ok ? 'Settings saved' : 'Settings failed', res.ok ? 'ok' : 'err');
  if (res.ok) $('setPassword').value = '';
}


async function saveWifiCredentials() {
  const ssid = $('setWifiSsid').value.trim();
  const password = $('setWifiPassword').value;
  if (!ssid) throw new Error('SSID required');
  const res = await api('/api/wifi', { method: 'POST', body: JSON.stringify({ ssid, password }) });
  toast(res.ok ? 'Wi-Fi credentials saved' : 'Wi-Fi save failed', res.ok ? 'ok' : 'err');
}

async function scanWifi() {
  const data = await api('/api/wifi?scan=1');
  const nets = data.scan || [];
  $('wifiScanList').innerHTML = nets
    .map((n) => `<div class="card"><strong>${n.ssid || '(hidden)'}</strong><div>RSSI ${n.rssi} dBm</div></div>`)
    .join('') || '<div class="muted">No networks found</div>';
}

function connectWebSocket() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  state.ws = new WebSocket(`${proto}//${location.host}/ws`);
  state.ws.onmessage = (event) => {
    try {
      const msg = JSON.parse(event.data);
      if (msg.type === 'status' && msg.payload) {
        state.system = msg.payload;
        renderMetrics(msg.payload);
      }
      if (msg.type === 'terminal') {
        window.tinyTerminalAppend(msg.output || '');
      }
    } catch {}
  };
}

function initClock() {
  setInterval(() => {
    $('clock').textContent = new Date().toLocaleTimeString();
  }, 1000);
}

async function handleLogin(evt) {
  evt.preventDefault();
  const username = $('username').value.trim() || 'admin';
  const password = $('password').value;
  const res = await api('/api/login', { method: 'POST', body: JSON.stringify({ username, password }) });
  if (!res.ok) {
    toast('Login failed', 'err');
    return;
  }
  showDesktop();
  await bootstrapDesktop();
  toast('Logged in');
}

async function bootstrapDesktop() {
  await loadSystem();
  await loadFiles();
  await loadSettings();
  connectWebSocket();
  window.tinyEditorInit();
  window.tinyTerminalInit();
  switchPage('dashboard');
}

async function checkSession() {
  try {
    const s = await api('/api/session');
    if (s.authenticated) {
      showDesktop();
      await bootstrapDesktop();
    } else {
      showLogin();
    }
  } catch {
    showLogin();
  }
}

function bindEvents() {
  $('loginForm').addEventListener('submit', (e) => handleLogin(e).catch((err) => toast(err.message, 'err')));
  $('logoutBtn').addEventListener('click', async () => {
    await api('/api/logout', { method: 'POST' });
    showLogin();
    toast('Logged out');
  });
  $('refreshBtn').addEventListener('click', () => Promise.all([loadSystem(), loadFiles(), loadSettings()]).then(() => toast('Refreshed')).catch((e) => toast(e.message, 'err')));

  document.querySelectorAll('.nav-btn').forEach((btn) => {
    btn.addEventListener('click', () => switchPage(btn.dataset.page));
  });

  $('reloadFiles').addEventListener('click', () => loadFiles().catch((e) => toast(e.message, 'err')));
  $('fileSearch').addEventListener('input', () => loadFiles().catch(() => {}));
  $('newFolderBtn').addEventListener('click', async () => {
    const name = prompt('Folder name (relative to current path):');
    if (!name) return;
    const base = $('filePath').value || '/';
    const path = `${base.replace(/\/$/, '')}/${name}`;
    const res = await api('/api/files', { method: 'POST', body: JSON.stringify({ action: 'mkdir', path }) });
    toast(res.ok ? 'Folder created' : 'Create failed', res.ok ? 'ok' : 'err');
    loadFiles();
  });
  $('newFileBtn').addEventListener('click', async () => {
    const name = prompt('File name (relative to current path):');
    if (!name) return;
    const base = $('filePath').value || '/';
    const path = `${base.replace(/\/$/, '')}/${name}`;
    const res = await api('/api/files', { method: 'POST', body: JSON.stringify({ action: 'create', path, content: '' }) });
    toast(res.ok ? 'File created' : 'Create failed', res.ok ? 'ok' : 'err');
    loadFiles();
  });

  $('fileUpload').addEventListener('change', (e) => uploadFiles([...e.target.files]).catch((err) => toast(err.message, 'err')));

  $('saveSettings').addEventListener('click', () => saveSettings().catch((e) => toast(e.message, 'err')));
  $('scanWifiBtn').addEventListener('click', () => scanWifi().catch((e) => toast(e.message, 'err')));
  $('saveWifiBtn').addEventListener('click', () => saveWifiCredentials().catch((e) => toast(e.message, 'err')));
  $('rebootBtn').addEventListener('click', async () => {
    await api('/api/reboot', { method: 'POST' });
    toast('Rebooting...');
  });
  $('factoryResetBtn').addEventListener('click', async () => {
    const confirmCode = prompt('Type RESET to confirm factory reset:');
    if (confirmCode !== 'RESET') return;
    const password = prompt('Enter current password to continue:') || '';
    await api('/api/settings', { method: 'POST', body: JSON.stringify({ factoryReset: true, confirm: confirmCode, password }) });
    toast('Factory reset complete. Reboot recommended.');
    await loadSettings();
  });

  $('otaUpload').addEventListener('click', async () => {
    const file = $('otaFile').files[0];
    if (!file) return toast('Select firmware .bin first', 'err');
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota');
    xhr.withCredentials = true;
    xhr.upload.onprogress = (e) => {
      if (!e.lengthComputable) return;
      $('otaProgress').style.width = `${Math.round((e.loaded / e.total) * 100)}%`;
    };
    xhr.onload = () => toast(xhr.status === 200 ? 'OTA complete. Rebooting.' : 'OTA failed', xhr.status === 200 ? 'ok' : 'err');
    const form = new FormData();
    form.append('file', file, file.name);
    xhr.send(form);
  });

  initDropzone();
}

window.addEventListener('load', () => {
  bindEvents();
  initClock();
  checkSession();
});

(function () {
  const area = () => document.getElementById('editorArea');
  const lines = () => document.getElementById('lineNumbers');
  const pathEl = () => document.getElementById('editorPath');

  let autosaveTimer;
  let undoStack = [''];
  let redoStack = [];
  let internalChange = false;

  function notify(message) {
    const wrap = document.getElementById('toastWrap');
    if (!wrap) return alert(message);
    const t = document.createElement('div');
    t.className = 'toast ok';
    t.textContent = message;
    wrap.appendChild(t);
    setTimeout(() => t.remove(), 2600);
  }

  function updateLineNumbers() {
    const count = Math.max(1, area().value.split('\n').length);
    lines().textContent = Array.from({ length: count }, (_, i) => i + 1).join('\n');
  }

  function pushUndoSnapshot(value) {
    if (undoStack[undoStack.length - 1] === value) return;
    undoStack.push(value);
    if (undoStack.length > 120) undoStack.shift();
    redoStack = [];
  }

  function applyEditorValue(value) {
    internalChange = true;
    area().value = value;
    updateLineNumbers();
    internalChange = false;
  }

  async function loadFile() {
    const path = pathEl().value.trim();
    if (!path) return;
    const res = await fetch(`/api/editor/load?path=${encodeURIComponent(path)}`, { credentials: 'include' });
    const data = await res.json();
    applyEditorValue(data.content || '');
    undoStack = [area().value];
    redoStack = [];
  }

  async function saveFile(pathOverride = null) {
    const path = (pathOverride || pathEl().value).trim();
    if (!path) throw new Error('Path required');
    pathEl().value = path;
    const res = await fetch('/api/editor/save', {
      method: 'POST',
      credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path, content: area().value }),
    });
    const data = await res.json();
    if (!data.ok) throw new Error('Save failed');
  }

  function undo() {
    if (undoStack.length < 2) return;
    const current = undoStack.pop();
    redoStack.push(current);
    applyEditorValue(undoStack[undoStack.length - 1]);
  }

  function redo() {
    if (!redoStack.length) return;
    const value = redoStack.pop();
    applyEditorValue(value);
    pushUndoSnapshot(value);
  }

  function init() {
    area().addEventListener('input', () => {
      if (!internalChange) pushUndoSnapshot(area().value);
      updateLineNumbers();
      clearTimeout(autosaveTimer);
      autosaveTimer = setTimeout(() => saveFile().catch(() => {}), 4000);
    });

    area().addEventListener('scroll', () => {
      lines().scrollTop = area().scrollTop;
    });

    document.getElementById('editorLoad').addEventListener('click', () => loadFile().catch((e) => alert(e.message)));
    document.getElementById('editorSave').addEventListener('click', async () => {
      try {
        await saveFile();
        notify('Saved');
      } catch (e) {
        alert(e.message);
      }
    });
    document.getElementById('editorSaveAs').addEventListener('click', async () => {
      const path = prompt('Save as path', pathEl().value || '/home/new.txt');
      if (!path) return;
      await saveFile(path);
      notify('Saved as new file');
    });
    document.getElementById('editorNew').addEventListener('click', () => {
      applyEditorValue('');
      pathEl().value = '/home/new.txt';
      undoStack = [''];
      redoStack = [];
    });
    document.getElementById('editorUndo').addEventListener('click', undo);
    document.getElementById('editorRedo').addEventListener('click', redo);

    window.addEventListener('keydown', async (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
        e.preventDefault();
        try {
          await saveFile();
          notify('Saved');
        } catch (err) {
          alert(err.message);
        }
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'o') {
        e.preventDefault();
        loadFile().catch((err) => alert(err.message));
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
        e.preventDefault();
        undo();
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'y') {
        e.preventDefault();
        redo();
      }
    });

    updateLineNumbers();
  }

  window.tinyEditorInit = init;
  window.tinyEditorLoad = loadFile;
})();

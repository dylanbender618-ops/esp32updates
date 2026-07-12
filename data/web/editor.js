(function () {
  const area = () => document.getElementById('editorArea');
  const lines = () => document.getElementById('lineNumbers');
  const pathEl = () => document.getElementById('editorPath');
  let autosaveTimer;

  function updateLineNumbers() {
    const count = Math.max(1, area().value.split('\n').length);
    lines().textContent = Array.from({ length: count }, (_, i) => i + 1).join('\n');
  }

  async function loadFile() {
    const path = pathEl().value.trim();
    if (!path) return;
    const res = await fetch(`/api/editor/load?path=${encodeURIComponent(path)}`, { credentials: 'include' });
    const data = await res.json();
    area().value = data.content || '';
    updateLineNumbers();
  }

  async function saveFile(pathOverride = null) {
    const path = (pathOverride || pathEl().value).trim();
    if (!path) return alert('Path required');
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

  function init() {
    area().addEventListener('input', () => {
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
        alert('Saved');
      } catch (e) {
        alert(e.message);
      }
    });
    document.getElementById('editorSaveAs').addEventListener('click', async () => {
      const path = prompt('Save as path', pathEl().value || '/home/new.txt');
      if (!path) return;
      await saveFile(path);
      alert('Saved');
    });
    document.getElementById('editorNew').addEventListener('click', () => {
      area().value = '';
      pathEl().value = '/home/new.txt';
      updateLineNumbers();
    });
    document.getElementById('editorUndo').addEventListener('click', () => document.execCommand('undo'));
    document.getElementById('editorRedo').addEventListener('click', () => document.execCommand('redo'));

    window.addEventListener('keydown', async (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
        e.preventDefault();
        try {
          await saveFile();
          alert('Saved');
        } catch (err) {
          alert(err.message);
        }
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'o') {
        e.preventDefault();
        loadFile().catch((err) => alert(err.message));
      }
    });

    updateLineNumbers();
  }

  window.tinyEditorInit = init;
  window.tinyEditorLoad = loadFile;
})();

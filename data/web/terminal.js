(function () {
  const output = () => document.getElementById('terminalOutput');
  const input = () => document.getElementById('terminalInput');
  let history = [];
  let historyIndex = -1;

  function append(text) {
    if (!text) return;
    output().textContent += `${text}\n`;
    output().scrollTop = output().scrollHeight;
  }

  async function runCommand(cmd) {
    append(`TinyPiOS $ ${cmd}`);
    const res = await fetch('/api/terminal', {
      method: 'POST',
      credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: cmd }),
    });
    const data = await res.json();
    if (data.clear) output().textContent = '';
    if (data.output) append(data.output);
  }

  function init() {
    const form = document.getElementById('terminalForm');
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      const cmd = input().value.trim();
      if (!cmd) return;
      history.push(cmd);
      historyIndex = history.length;
      input().value = '';
      try {
        await runCommand(cmd);
      } catch (err) {
        append(`Error: ${err.message}`);
      }
    });

    input().addEventListener('keydown', (e) => {
      if (e.key === 'ArrowUp') {
        if (history.length === 0) return;
        historyIndex = Math.max(0, historyIndex - 1);
        input().value = history[historyIndex] || '';
        e.preventDefault();
      } else if (e.key === 'ArrowDown') {
        if (history.length === 0) return;
        historyIndex = Math.min(history.length, historyIndex + 1);
        input().value = history[historyIndex] || '';
        e.preventDefault();
      }
    });

    append('Welcome to TinyPiOS terminal. Type help for commands.');
  }

  window.tinyTerminalInit = init;
  window.tinyTerminalAppend = append;
})();

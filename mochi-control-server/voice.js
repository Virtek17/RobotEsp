const {spawn} = require('node:child_process');
const path = require('node:path');

class LaptopVoice {
  constructor({spawnImpl = spawn} = {}) {
    this.spawn = spawnImpl;
    this.enabled = true;
    this.child = null;
    this.error = '';
    this.lastKey = '';
  }
  status() { return {enabled: this.enabled, speaking: Boolean(this.child), error: this.error}; }
  setEnabled(enabled) { this.enabled = enabled; if (!enabled) this.stop(); }
  stop() { if (this.child) this.child.kill(); }
  speak(text, key) {
    if (!this.enabled || !text || key === this.lastKey) return Promise.resolve();
    this.lastKey = key;
    this.stop();
    this.error = '';
    return new Promise((resolve, reject) => {
      const child = this.spawn('powershell.exe', ['-NoProfile', '-NonInteractive',
        '-File', path.join(__dirname, 'speak.ps1')], {windowsHide: true, stdio: ['pipe', 'ignore', 'pipe']});
      this.child = child;
      let stderr = '';
      let stopped = false;
      let settled = false;
      const originalStop = child.kill.bind(child);
      child.kill = (...args) => { stopped = true; return originalStop(...args); };
      child.stderr.on('data', data => { stderr = (stderr + data.toString('utf8')).slice(-500); });
      const timer = setTimeout(() => { child.kill(); this.error = 'Превышено время озвучивания'; }, 45000);
      const finish = error => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        if (this.child === child) this.child = null;
        if (error && !stopped) { this.error = 'Не удалось озвучить ответ: проверьте русский голос Windows и аудиовыход'; reject(new Error(this.error)); }
        else resolve();
      };
      child.once('error', finish);
      child.once('exit', code => finish(code === 0 ? null : new Error(stderr || 'TTS failed')));
      child.stdin.on('error', () => {});
      child.stdin.end(JSON.stringify({text}));
    });
  }
}
module.exports = {LaptopVoice};

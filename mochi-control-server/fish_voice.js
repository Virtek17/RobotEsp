const fs = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {LaptopVoice} = require('./voice');

function settings() {
  let values = {};
  try {
    for (const line of fs.readFileSync(path.join(__dirname, '.env'), 'utf8').split(/\r?\n/)) {
      const m = line.match(/^\s*(FISH_AUDIO_API_KEY|FISH_AUDIO_VOICE_ID)\s*=\s*(.*?)\s*$/);
      if (m) values[m[1]] = m[2].replace(/^(['"])(.*)\1$/, '$2');
    }
  } catch (e) { if (e.code !== 'ENOENT') throw e; }
  return {key: process.env.FISH_AUDIO_API_KEY || values.FISH_AUDIO_API_KEY || '',
    id: process.env.FISH_AUDIO_VOICE_ID || values.FISH_AUDIO_VOICE_ID || ''};
}

function playWav(wav, signal) {
  return new Promise((resolve, reject) => {
    if (signal.aborted) { resolve(); return; }
    const child = spawn('powershell.exe', ['-NoProfile', '-NonInteractive', '-File',
      path.join(__dirname, 'play_wav.ps1')], {windowsHide: true, stdio: ['pipe', 'ignore', 'ignore']});
    const cancel = () => { child.kill(); };
    signal.addEventListener('abort', cancel, {once: true});
    let done = false;
    const finish = error => {
      if (done) return; done = true;
      signal.removeEventListener('abort', cancel);
      error && !signal.aborted ? reject(error) : resolve();
    };
    child.once('error', () => finish(new Error('Не удалось запустить аудиоплеер Windows')));
    child.once('exit', code => finish(code === 0 ? null : new Error('Не удалось воспроизвести WAV Fish Audio')));
    child.stdin.on('error', () => {});
    child.stdin.end(JSON.stringify({wav: wav.toString('base64')}));
  });
}

function normalizeWav(input) {
  if (input.length < 44 || input.toString('ascii',0,4) !== 'RIFF' || input.toString('ascii',8,12) !== 'WAVE') {
    throw new Error('Fish Audio вернул неподдерживаемый аудиоформат');
  }
  let fmt = null, audio = null;
  for (let p = 12; p + 8 <= input.length;) {
    const tag = input.toString('ascii', p, p+4);
    const declared = input.readUInt32LE(p+4);
    const start = p+8;
    if (tag === 'data') {
      // Streaming WAV uses near-UINT32_MAX placeholder sizes; SoundPlayer
      // expects a complete finite WAV. The response body is already complete.
      audio = input.subarray(start, start + Math.min(declared, input.length-start));
      break;
    }
    if (declared > input.length-start) throw new Error('Повреждён заголовок WAV Fish Audio');
    if (tag === 'fmt ') fmt = input.subarray(start,start+declared);
    p = start+declared+(declared%2);
  }
  if (!fmt || fmt.length < 16 || !audio || audio.length === 0 ||
      fmt.readUInt16LE(0) !== 1 || fmt.readUInt16LE(2) !== 1 || fmt.readUInt16LE(14) !== 16 || audio.length%2) {
    throw new Error('Ожидается WAV PCM 16 бит, моно');
  }
  const rate = fmt.readUInt32LE(4);
  if (![8000,16000,24000,32000,44100,48000].includes(rate)) throw new Error('Неподдерживаемая частота WAV');
  const output = Buffer.alloc(44+audio.length);
  output.write('RIFF'); output.writeUInt32LE(output.length-8,4); output.write('WAVEfmt ',8);
  output.writeUInt32LE(16,16); output.writeUInt16LE(1,20); output.writeUInt16LE(1,22);
  output.writeUInt32LE(rate,24); output.writeUInt32LE(rate*2,28);
  output.writeUInt16LE(2,32); output.writeUInt16LE(16,34);
  output.write('data',36); output.writeUInt32LE(audio.length,40); audio.copy(output,44);
  return output;
}

class FishVoice {
  constructor({getSettings = settings, fetchImpl = fetch, play = playWav} = {}) {
    this.settings = getSettings; this.fetch = fetchImpl; this.play = play;
    this.enabled = true; this.error = ''; this.lastKey = ''; this.active = null;
  }
  status() { return {enabled: this.enabled, speaking: Boolean(this.active), error: this.error,
    provider: 'fish', model: 's2.1-pro-free', configured: Boolean(this.settings().key && this.settings().id)}; }
  setEnabled(value) { this.enabled = value; if (!value) this.stop(); }
  stop() { this.active?.abort(); }
  async speak(text, captureKey) {
    if (!this.enabled || !text || captureKey === this.lastKey) return;
    const {key, id} = this.settings();
    if (!key || !id) throw new Error('Заполните ключ и ID голоса Fish Audio в .env');
    if (!/^[a-f0-9]{32}$/i.test(id)) throw new Error('Некорректный ID голоса Fish Audio');
    this.lastKey = captureKey;
    this.stop();
    const controller = new AbortController();
    this.active = controller; this.error = '';
    let timedOut = false;
    const timer = setTimeout(() => { timedOut = true; controller.abort(); }, 40000);
    try {
      const response = await this.fetch('https://api.fish.audio/v1/tts', {
        method: 'POST', signal: controller.signal,
        headers: {'Authorization': `Bearer ${key}`, 'Content-Type': 'application/json', model: 's2.1-pro-free'},
        body: JSON.stringify({text, reference_id: id, format: 'wav', sample_rate: 24000,
          latency: 'balanced', normalize: true, language: 'ru'}),
      });
      if (!response.ok) throw new Error(response.status === 401 ? 'Проверьте API-ключ Fish Audio' :
        response.status === 402 ? 'Бесплатный API Fish Audio недоступен для аккаунта; платный режим не включён' :
        response.status === 429 ? 'Лимит Fish Audio: повторите позже' : `Fish Audio: HTTP ${response.status}`);
      const length = Number(response.headers.get('content-length'));
      if (length > 5000000) throw new Error('Слишком большой ответ Fish Audio');
      const chunks = []; let size = 0;
      for await (const chunk of response.body) {
        size += chunk.length;
        if (size > 5000000) { controller.abort(); throw new Error('Слишком большой ответ Fish Audio'); }
        chunks.push(chunk);
      }
      const wav = normalizeWav(Buffer.concat(chunks));
      if (!controller.signal.aborted) await this.play(wav, controller.signal);
    } catch (error) {
      if (timedOut) { this.error = 'Fish Audio не ответил вовремя'; throw new Error(this.error); }
      if (!controller.signal.aborted || error.message === 'Слишком большой ответ Fish Audio') {
        if (error instanceof TypeError && /fetch failed|terminated/i.test(error.message)) {
          const code = error.cause?.code;
          const reasons = {
            ENOTFOUND: 'не удалось найти сервер (DNS)',
            EAI_AGAIN: 'временная ошибка DNS',
            ECONNRESET: 'соединение сброшено',
            ECONNREFUSED: 'соединение отклонено',
            ETIMEDOUT: 'тайм-аут соединения',
            UND_ERR_CONNECT_TIMEOUT: 'тайм-аут подключения',
            UND_ERR_SOCKET: 'соединение оборвалось',
            CERT_HAS_EXPIRED: 'истёк сертификат TLS',
            UNABLE_TO_VERIFY_LEAF_SIGNATURE: 'не удалось проверить сертификат TLS',
          };
          this.error = 'Сетевая ошибка Fish Audio: ' + (reasons[code] || 'не удалось получить аудио') +
            '. Проверьте интернет и VPN/прокси';
          throw new Error(this.error);
        }
        this.error = error.message; throw error;
      }
    } finally {
      clearTimeout(timer);
      if (this.active === controller) this.active = null;
    }
  }
}

class ConfigurableVoice {
  constructor() { this.local = new LaptopVoice(); this.fish = new FishVoice(); this.enabled = true; }
  selected() { return this.fish.status().configured ? this.fish : this.local; }
  status() { const selected = this.selected(); return {...selected.status(), enabled: this.enabled,
    speaking: this.local.status().speaking || this.fish.status().speaking,
    provider: selected === this.fish ? 'fish' : 'windows'}; }
  setEnabled(value) { this.enabled = value; this.local.setEnabled(value); this.fish.setEnabled(value); }
  stop() { this.local.stop(); this.fish.stop(); }
  speak(text, key) { return this.selected().speak(text, key); }
}
module.exports = {FishVoice, ConfigurableVoice, normalizeWav};

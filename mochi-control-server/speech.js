const {spawn} = require('node:child_process');
const path = require('node:path');
const readline = require('node:readline');

class LocalRecognizer {
  constructor() {
    this.ready = false;
    this.pending = null;
    this.error = '';
    const python = process.env.MOCHI_PYTHON || path.join(__dirname, '.venv',
      process.platform === 'win32' ? 'Scripts/python.exe' : 'bin/python');
    this.child = spawn(python, [path.join(__dirname, 'speech_worker.py')], {
      windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'],
    });
    readline.createInterface({input: this.child.stdout}).on('line', line => {
      try {
        const result = JSON.parse(line);
        if (result.ready) { this.ready = true; this.error = ''; return; }
        if (this.pending) {
          const pending = this.pending;
          this.pending = null;
          clearTimeout(pending.timer);
          result.error ? pending.reject(new Error(result.error)) : pending.resolve(result.text || '');
        }
      } catch (error) { this.error = error.message; }
    });
    this.child.stderr.on('data', data => { this.error = data.toString().slice(-500); });
    const failed = error => {
      this.ready = false;
      this.error = error.message;
      if (this.pending) {
        clearTimeout(this.pending.timer);
        this.pending.reject(error);
        this.pending = null;
      }
    };
    this.child.on('error', failed);
    this.child.stdin.on('error', failed);
    this.child.on('exit', code => failed(new Error(`Vosk stopped (${code}). ${this.error}`)));
  }

  recognize(wav) {
    if (!this.ready || this.pending) return Promise.reject(new Error('Vosk is not ready'));
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.ready = false;
        this.child.kill();
        // Exit handler rejects the request, preventing stale replies from being reused.
      }, 30000);
      this.pending = {resolve, reject, timer};
      this.child.stdin.write(JSON.stringify({wav: wav.toString('base64')}) + '\n');
    });
  }

  close() { this.child.kill(); }
}

function createSpeechBridge({getIp, recognizer, assistant = null, voice = null, fetchImpl = fetch}) {
  const status = {engine: 'Vosk local / Russian', phase: 'starting', text: '', answer: '', emotion: 'alive', voiceError: '', aiError: '', error: '', timings: null};
  let running = false;
  let delivery = null;
  let lastKey = '';
  let voiceKey = '';
  async function request(ip, route, options = {}) {
    const response = await fetchImpl(`http://${ip}${route}`, {
      signal: AbortSignal.timeout(5000), ...options,
    });
    if (!response.ok) {
      const error = new Error(`ESP32 HTTP ${response.status}`);
      error.status = response.status;
      throw error;
    }
    return response;
  }
  async function progress(ip, id, stage) {
    try {
      await request(ip, '/api/stt/progress', {method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams({id: String(id), stage}),
        signal: AbortSignal.timeout(1500)});
    } catch { /* Older firmware or lost progress must not prevent result delivery. */ }
  }
  async function tick() {
    if (running) return;
    running = true;
    try {
      if (!recognizer.ready) {
        status.phase = 'starting'; status.error = recognizer.error || ''; return;
      }
      const ip = getIp();
      if (!ip) { status.phase = 'waiting_for_ip'; return; }
      if (delivery && delivery.ip !== ip) delivery = null;
      if (!delivery) {
        const state = await (await request(ip, '/api/stt/state')).json();
        status.phase = state.phase;
        if (state.phase === 'recording' || state.touch) {
          if (voiceKey && state.phase === 'text') progress(ip, state.id, 'answered');
          voiceKey = ''; voice?.stop();
        }
        if (state.phase === 'text' && voice?.status().speaking) status.phase = 'speaking';
        status.error = '';
        const key = `${ip}:${state.id}`;
        if (state.phase !== 'ready' || key === lastKey) return;
        if (!Number.isSafeInteger(state.id) || state.id < 0) throw new Error('Invalid capture id');
        const started = performance.now();
        status.timings = null;
        const response = await request(ip, `/api/stt/audio?id=${state.id}`);
        const wav = Buffer.from(await response.arrayBuffer());
        if (wav.length < 44 || wav.length > 128044 || wav.toString('ascii', 0, 4) !== 'RIFF') {
          throw new Error('Invalid WAV from ESP32');
        }
        status.phase = 'recognizing';
        await progress(ip, state.id, 'recognizing');
        const downloaded = performance.now();
        let text;
        try { text = await recognizer.recognize(wav); }
        catch (error) {
          delivery = {ip, id: state.id, key, text: 'Ошибка распознавания на ПК', emotion: 'confused', error: true};
          throw error;
        }
        status.text = text;
        const recognized = performance.now();
        let resultText = text || 'Речь не распознана';
        status.answer = '';
        status.aiError = '';
        status.emotion = 'alive';
        status.voiceError = '';
        if (text && assistant?.status().configured) {
          status.phase = 'thinking';
          await progress(ip, state.id, 'thinking');
          try {
            const reply = assistant.reply ? await assistant.reply(text) :
              {text: await assistant.answer(text), emotion: 'alive'};
            status.answer = reply.text;
            status.emotion = reply.emotion;
            resultText = status.answer;
          } catch (error) {
            status.aiError = error.message;
            status.emotion = 'confused';
            resultText = `ИИ недоступен. ${error.message}`;
          }
        }
        const answered = performance.now();
        // Truncate by code point to fit the board's 512-byte form limit.
        let oledText = '';
        for (const char of resultText) {
          if (Buffer.byteLength(oledText + char) > 500) break;
          oledText += char;
        }
        delivery = {ip, id: state.id, key, text: oledText, started,
          emotion: status.emotion, error: Boolean(status.aiError) || !text,
          spokenText: status.aiError ? '' : status.answer,
          downloadMs: Math.round(downloaded - started),
          recognizeMs: Math.round(recognized - downloaded),
          aiMs: Math.round(answered - recognized)};
      }
      const delivering = performance.now();
      await request(delivery.ip, '/api/stt/result', {
        method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams({id: String(delivery.id), text: delivery.text,
          emotion: delivery.emotion || 'alive', error: delivery.error ? '1' : '0'}),
      });
      if (delivery.started !== undefined) {
        status.timings = {downloadMs: delivery.downloadMs, recognizeMs: delivery.recognizeMs,
          aiMs: delivery.aiMs,
          deliveryMs: Math.round(performance.now() - delivering),
          totalMs: Math.round(performance.now() - delivery.started)};
        console.log('STT timing:', JSON.stringify(status.timings));
      }
      lastKey = delivery.key;
      const completed = delivery;
      delivery = null;
      status.phase = 'text';
      status.error = '';
      if (voice && completed.spokenText && voice.status().enabled) {
        voiceKey = completed.key;
        progress(completed.ip, completed.id, 'speaking').then(async () => {
          if (voiceKey !== completed.key) return;
          const voiceStartedAt = performance.now();
          console.log('Voice: starting', voice.status().provider || 'windows');
          await voice.speak(completed.spokenText, completed.key);
          console.log('Voice: finished or stopped, ms:', Math.round(performance.now()-voiceStartedAt));
        }).catch(error => {
          status.voiceError = error.message;
          console.error('Voice error:', error.message);
        }).finally(() => {
          if (voiceKey === completed.key) {
            progress(completed.ip, completed.id, 'answered');
            voiceKey = '';
          }
        });
      }
    } catch (error) {
      status.phase = 'error';
      status.error = error.status === 404 ? 'Обновите прошивку ESP32: API речи пока отсутствует' :
        error.name === 'TimeoutError' ? 'Плата не отвечает: проверьте питание, IP и доступ по сети' : error.message;
      if (error.status === 409) delivery = null;
    } finally { running = false; }
  }
  return {status, tick};
}

module.exports = {LocalRecognizer, createSpeechBridge};

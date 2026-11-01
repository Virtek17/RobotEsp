const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const net = require('node:net');
const {LocalRecognizer, createSpeechBridge} = require('./speech');
const {createAssistant} = require('./ai');
const assistant = createAssistant();
const {ConfigurableVoice} = require('./fish_voice');
const voice = new ConfigurableVoice();

const port = Number(process.env.PORT || 3000);
const configPath = path.join(__dirname, 'config.json');
const pagePath = path.join(__dirname, 'public', 'index.html');
let deviceIp = '';
let speech;

function localIp(ip) {
  if (net.isIP(ip) !== 4) return false;
  const parts = ip.split('.').map(Number);
  return parts[0] === 10 ||
    (parts[0] === 172 && parts[1] >= 16 && parts[1] <= 31) ||
    (parts[0] === 192 && parts[1] === 168);
}

function json(res, status, value) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  res.end(JSON.stringify(value));
}

async function body(req) {
  let raw = '';
  for await (const chunk of req) {
    raw += chunk;
    if (raw.length > 4096) throw new Error('Слишком большой запрос');
  }
  return JSON.parse(raw || '{}');
}

async function deviceRequest(route, options = {}) {
  if (!deviceIp) throw new Error('Сначала укажите IP платы');
  const response = await fetch(`http://${deviceIp}${route}`, {
    ...options,
    signal: AbortSignal.timeout(4000),
  });
  const payload = await response.json();
  if (!response.ok) throw new Error(payload.error || `Ошибка ESP32: ${response.status}`);
  return payload;
}

async function handle(req, res) {
  const url = new URL(req.url, `http://localhost:${port}`);
  if (req.method === 'GET' && url.pathname === '/') {
    res.writeHead(200, {'Content-Type': 'text/html; charset=utf-8'});
    res.end(await fs.readFile(pagePath));
  } else if (req.method === 'GET' && url.pathname === '/api/config') {
    json(res, 200, {ip: deviceIp});
  } else if (req.method === 'GET' && url.pathname === '/api/stt') {
    json(res, 200, {...speech.status, ai: assistant.status(), voice: voice.status()});
  } else if (req.method === 'POST' && url.pathname === '/api/voice') {
    const {enabled, stop} = await body(req);
    if (enabled !== undefined && typeof enabled !== 'boolean') {
      return json(res, 400, {error: 'enabled must be boolean'});
    }
    if (enabled !== undefined) voice.setEnabled(enabled);
    if (stop === true) voice.stop();
    json(res, 200, voice.status());
  } else if (req.method === 'POST' && url.pathname === '/api/ai/reset') {
    assistant.reset();
    json(res, 200, {ok: true});
  } else if (req.method === 'POST' && url.pathname === '/api/config') {
    const {ip} = await body(req);
    if (typeof ip !== 'string' || !localIp(ip)) {
      return json(res, 400, {error: 'Укажите локальный IPv4 адрес платы'});
    }
    deviceIp = ip;
    await fs.writeFile(configPath, JSON.stringify({ip}, null, 2) + '\n');
    json(res, 200, {ip: deviceIp});
  } else if (req.method === 'GET' && url.pathname === '/api/emotions') {
    json(res, 200, await deviceRequest('/api/emotions'));
  } else if (req.method === 'POST' && url.pathname === '/api/emotion') {
    const {emotion} = await body(req);
    if (typeof emotion !== 'string' || !/^[a-z0-9_]{1,32}$/.test(emotion)) {
      return json(res, 400, {error: 'Некорректное имя эмоции'});
    }
    json(res, 200, await deviceRequest('/api/emotion', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({emotion}),
    }));
  } else {
    json(res, 404, {error: 'Не найдено'});
  }
}

async function main() {
  try {
    const saved = JSON.parse(await fs.readFile(configPath, 'utf8'));
    if (localIp(saved.ip)) deviceIp = saved.ip;
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }

  const recognizer = new LocalRecognizer();
  speech = createSpeechBridge({getIp: () => deviceIp, recognizer, assistant, voice});
  const poller = setInterval(() => speech.tick(), 150);
  const close = () => { clearInterval(poller); recognizer.close(); voice.stop(); process.exit(0); };
  process.once('SIGINT', close);
  process.once('SIGTERM', close);
  const httpServer = http.createServer((req, res) => {
    handle(req, res).catch((error) => {
      const status = error instanceof SyntaxError ? 400 : 502;
      json(res, status, {error: error.message});
    });
  }).listen(port, '127.0.0.1', () => {
    console.log(`Mochi control: http://localhost:${port}`);
  });
  httpServer.on('error', error => {
    clearInterval(poller);
    recognizer.close();
    voice.stop();
    console.error(error.message);
    process.exitCode = 1;
  });
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});

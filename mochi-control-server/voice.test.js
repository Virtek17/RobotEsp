const test = require('node:test');
const assert = require('node:assert/strict');
const {EventEmitter} = require('node:events');
const {LaptopVoice} = require('./voice');
const {createSpeechBridge} = require('./speech');

test('TTS passes text as stdin data, deduplicates and can be interrupted', async () => {
  const children = [];
  const voice = new LaptopVoice({spawnImpl: (exe, args, options) => {
    assert.equal(exe, 'powershell.exe');
    assert.ok(args.includes('-File'));
    assert.ok(!args.includes('-Command'));
    assert.equal(options.windowsHide, true);
    const child = new EventEmitter(); child.stderr = new EventEmitter();
    child.stdin = new EventEmitter(); child.stdin.end = text => { child.input = JSON.parse(text); };
    child.kill = () => { child.emit('exit', null); return true; };
    children.push(child); return child;
  }});
  const promise = voice.speak('Привет! $() " текст', 'one');
  assert.equal(children[0].input.text, 'Привет! $() " текст');
  assert.equal(voice.status().speaking, true);
  await voice.speak('Привет', 'one');
  assert.equal(children.length, 1);
  voice.setEnabled(false);
  await promise;
  assert.equal(voice.status().speaking, false);
  await voice.speak('Не произносить', 'two');
  assert.equal(children.length, 1);
});

test('emotion, stages and voice are delivered once despite a result retry', async () => {
  let sends = 0, spoken = 0;
  const stages = [];
  const wav = Buffer.alloc(44); wav.write('RIFF');
  const bridge = createSpeechBridge({getIp: () => '192.168.0.100',
    recognizer: {ready: true, recognize: async () => 'я рад'},
    assistant: {status: () => ({configured: true}), reply: async () => ({text: 'Я рад за тебя!', emotion: 'happy'})},
    voice: {status: () => ({enabled: true, speaking: false}), stop() {}, speak: async text => {
      assert.equal(text, 'Я рад за тебя!'); spoken++;
    }},
    fetchImpl: async (url, options) => {
      if (url.endsWith('/state')) return new Response(JSON.stringify({phase: 'ready', id: 5}));
      if (url.includes('/audio?')) return new Response(wav);
      const form = new URLSearchParams(options.body);
      if (url.endsWith('/progress')) { stages.push(form.get('stage')); return new Response('{}'); }
      assert.equal(form.get('emotion'), 'happy');
      assert.equal(form.get('error'), '0');
      if (++sends === 1) throw new Error('retry me');
      return new Response('{}');
    }});
  await bridge.tick(); await bridge.tick();
  await new Promise(resolve => setImmediate(resolve));
  await bridge.tick();
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(spoken, 1);
  assert.deepEqual(stages, ['recognizing', 'thinking', 'speaking', 'answered']);
  assert.equal(bridge.status.emotion, 'happy');
});

test('a fresh touch cancels playback before a new recording starts', async () => {
  let touched = false, stopped = 0, completeVoice;
  const wav = Buffer.alloc(44); wav.write('RIFF');
  const bridge = createSpeechBridge({getIp: () => '192.168.0.100',
    recognizer: {ready: true, recognize: async () => 'привет'},
    assistant: {status: () => ({configured: true}), reply: async () => ({text: 'Привет!', emotion: 'happy'})},
    voice: {status: () => ({enabled: true, speaking: Boolean(completeVoice)}),
      stop() { stopped++; completeVoice?.(); },
      speak: () => new Promise(resolve => { completeVoice = resolve; })},
    fetchImpl: async url => {
      if (url.endsWith('/state')) return new Response(JSON.stringify({phase: touched ? 'text' : 'ready', id: 7, touch: touched}));
      if (url.includes('/audio?')) return new Response(wav);
      return new Response('{}');
    }});
  await bridge.tick();
  await new Promise(resolve => setImmediate(resolve));
  assert.ok(completeVoice);
  touched = true;
  await bridge.tick();
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(stopped, 1);
});

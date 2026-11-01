const test = require('node:test');
const assert = require('node:assert/strict');
const {createSpeechBridge, LocalRecognizer} = require('./speech');

test('capture is recognized once, failed result delivery is retried, Cyrillic survives', async () => {
  let recognitionCount = 0;
  let deliveries = 0;
  const wav = Buffer.alloc(44);
  wav.write('RIFF');
  const bridge = createSpeechBridge({
    getIp: () => '192.168.0.100',
    recognizer: {ready: true, recognize: async () => { recognitionCount++; return 'привет робот'; }},
    fetchImpl: async (url, options) => {
      if (url.endsWith('/state')) return new Response(JSON.stringify({phase: 'ready', id: 42}));
      if (url.includes('/audio?')) return new Response(wav);
      assert.equal(new URLSearchParams(options.body).get('text'), 'привет робот');
      if (++deliveries === 1) throw new Error('connection lost');
      return new Response('{}');
    },
  });
  await bridge.tick();
  assert.equal(bridge.status.error, 'connection lost');
  await bridge.tick();
  await bridge.tick();
  assert.equal(recognitionCount, 1);
  assert.equal(deliveries, 2);
  assert.equal(bridge.status.text, 'привет робот');
});

test('real local Russian model loads and accepts a silent 16kHz WAV', {timeout: 60000}, async () => {
  const recognizer = new LocalRecognizer();
  try {
    const deadline = Date.now() + 30000;
    while (!recognizer.ready && Date.now() < deadline) {
      if (recognizer.child.exitCode !== null) throw new Error(recognizer.error);
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    assert.ok(recognizer.ready, recognizer.error || 'model did not load');
    const wav = Buffer.alloc(44 + 32000);
    wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8);
    wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(1, 22);
    wav.writeUInt32LE(16000, 24); wav.writeUInt32LE(32000, 28);
    wav.writeUInt16LE(2, 32); wav.writeUInt16LE(16, 34);
    wav.write('data', 36); wav.writeUInt32LE(32000, 40);
    assert.equal(await recognizer.recognize(wav), '');
    await assert.rejects(recognizer.recognize(Buffer.from('not a wav')));
    assert.equal(await recognizer.recognize(wav), '');
  } finally { recognizer.close(); }
});

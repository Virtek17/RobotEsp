const test = require('node:test');
const assert = require('node:assert/strict');
const {FishVoice, normalizeWav} = require('./fish_voice');
const getSettings = () => ({key:'test-secret', id:'bc8eb8dcdc184763b0a769ee03275724'});

test('Fish network failures explain the cause without exposing transport details', async () => {
  let calls = 0;
  const voice = new FishVoice({getSettings, fetchImpl: async () => {
    calls++;
    throw new TypeError('fetch failed', {cause: Object.assign(new Error('test-secret'), {code:'ECONNRESET'})});
  }});
  await assert.rejects(voice.speak('Привет', 'network'), /соединение сброшено/);
  assert.equal(calls, 1);
  assert.equal(voice.status().speaking, false);
  assert.ok(!voice.error.includes('test-secret'));
});
test('Fish uses only the explicit free model and plays a validated WAV once', async () => {
  let calls = 0, plays = 0;
  const wav = Buffer.alloc(48); wav.write('RIFF'); wav.writeUInt32LE(40,4); wav.write('WAVEfmt ',8);
  wav.writeUInt32LE(16,16); wav.writeUInt16LE(1,20); wav.writeUInt16LE(1,22);
  wav.writeUInt32LE(24000,24); wav.writeUInt32LE(48000,28);
  wav.writeUInt16LE(2,32); wav.writeUInt16LE(16,34); wav.write('data',36); wav.writeUInt32LE(4,40);
  const voice = new FishVoice({getSettings, fetchImpl: async (url, options) => {
    calls++; assert.equal(url,'https://api.fish.audio/v1/tts');
    assert.equal(options.headers.model,'s2.1-pro-free');
    assert.equal(JSON.parse(options.body).reference_id,getSettings().id);
    return new Response(wav);
  }, play: async received => { assert.deepEqual(received,wav); plays++; }});
  await voice.speak('Привет!', 'one'); await voice.speak('Привет!', 'one');
  assert.equal(calls,1); assert.equal(plays,1);
  assert.ok(!JSON.stringify(voice.status()).includes('test-secret'));
});
test('Fish streaming WAV placeholder sizes become real lengths without changing samples', () => {
  const wav=Buffer.alloc(48); wav.write('RIFF'); wav.writeUInt32LE(4294967076,4); wav.write('WAVEfmt ',8);
  wav.writeUInt32LE(16,16); wav.writeUInt16LE(1,20); wav.writeUInt16LE(1,22);
  wav.writeUInt32LE(24000,24); wav.writeUInt16LE(2,32); wav.writeUInt16LE(16,34);
  wav.write('data',36); wav.writeUInt32LE(4294967040,40); wav.writeInt16LE(1234,44); wav.writeInt16LE(-1234,46);
  const fixed=normalizeWav(wav);
  assert.equal(fixed.readUInt32LE(4),40); assert.equal(fixed.readUInt32LE(40),4);
  assert.deepEqual(fixed.subarray(44),wav.subarray(44));
  const truncated=wav.subarray(0,47);
  assert.throws(()=>normalizeWav(truncated),/PCM/);
});
test('Fish payment errors do not fall back to a paid model or echo private response', async () => {
  let calls=0;
  const voice = new FishVoice({getSettings, fetchImpl: async () => { calls++; return new Response('test-secret',{status:402}); }});
  await assert.rejects(voice.speak('Привет','two'),/платный режим не включён/);
  assert.equal(calls,1); assert.ok(!voice.error.includes('test-secret'));
});

const test = require('node:test');
const assert = require('node:assert/strict');
const {createAssistant, cleanAnswer, parseReply} = require('./ai');
const {createSpeechBridge} = require('./speech');

test('AI emotions are validated and reasoning is filtered inside JSON', () => {
  assert.deepEqual(parseReply('{"text":"Я рад за тебя!","emotion":"happy"}'), {text:'Я рад за тебя!', emotion:'happy'});
  assert.deepEqual(parseReply('```json\n{"text":"Привет","emotion":"not-a-face"}\n```'), {text:'Привет', emotion:'alive'});
  assert.throws(() => parseReply('{"text":"Analyze the Request: private","emotion":"happy"}'), /готовую реплику/);
  assert.throws(() => parseReply('{"text":"Привет","emotion":"happy"}', 'length'), /готовую реплику/);
});

test('free-only model guard blocks requests before any network call', async () => {
  let called = false;
  const ai = createAssistant({settings: () => ({key: 'test-secret', model: 'paid/model'}),
    fetchImpl: async () => { called = true; }});
  await assert.rejects(ai.answer('привет'), /только/);
  assert.equal(called, false);
  assert.equal(JSON.stringify(ai.status()).includes('test-secret'), false);
});

test('OpenRouter requests contain bounded history; reset clears it', async () => {
  const requests = [];
  const ai = createAssistant({settings: () => ({key: 'test-secret', model: 'openrouter/free'}),
    fetchImpl: async (url, options) => {
      assert.equal(url, 'https://openrouter.ai/api/v1/chat/completions');
      assert.equal(options.headers.Authorization, 'Bearer test-secret');
      requests.push(JSON.parse(options.body));
      return new Response(JSON.stringify({choices: [{message: {content: 'Привет! Я Мочи.'}}]}));
    }});
  for (let i = 0; i < 6; i++) await ai.answer('Как дела?');
  assert.equal(requests[0].model, 'openrouter/free');
  assert.equal(requests[0].models, undefined);
  assert.deepEqual(requests[0].reasoning, {exclude: true});
  assert.equal(requests[1].messages[2].role, 'assistant');
  assert.equal(requests[5].messages.length, 8);
  ai.reset();
  await ai.answer('Привет');
  assert.equal(requests.at(-1).messages.length, 2);
});

test('reasoning in content is rejected; completed tagged reasoning is removed', () => {
  assert.throws(() => cleanAnswer('1. **Analyze the Request:**\n * Persona: robot', 'length'), /готовую реплику/);
  assert.throws(() => cleanAnswer('<think>unfinished internal analysis'), /готовую реплику/);
  assert.throws(() => cleanAnswer('Оборванный ответ', 'length'), /готовую реплику/);
  assert.equal(cleanAnswer('<think>private scratchpad</think>Привет!'), 'Привет!');
  assert.equal(cleanAnswer('**Я Мочи.**'), 'Я Мочи.');
  assert.ok([...cleanAnswer('Длинная фраза '.repeat(40))].length <= 220);
});

test('empty content never falls back to reasoning or pollutes conversation history', async () => {
  let calls = 0;
  const ai = createAssistant({settings: () => ({key: 'test-secret', model: 'openrouter/free'}),
    fetchImpl: async (url, options) => {
      const body = JSON.parse(options.body);
      if (++calls === 1) return new Response(JSON.stringify({choices: [{message: {content: '', reasoning: 'private'}}]}));
      assert.equal(body.messages.length, 2);
      return new Response(JSON.stringify({choices: [{message: {content: 'Привет!'}}]}));
    }});
  assert.equal(await ai.answer('Привет'), 'Привет!');
  assert.equal(calls, 2);
});

test('truncated answers get one larger-budget retry with a shared deadline', async () => {
  const requests = [], signals = [];
  const ai = createAssistant({settings: () => ({key:'test-secret', model:'openrouter/free'}),
    fetchImpl: async (url, options) => {
      requests.push(JSON.parse(options.body)); signals.push(options.signal);
      return new Response(JSON.stringify({choices:[{finish_reason:'length', message:{content:null, reasoning:'private'}}]}));
    }});
  await assert.rejects(ai.reply('Привет'), /готовый ответ/);
  assert.equal(requests.length, 2);
  assert.equal(requests[0].max_tokens, 2048);
  assert.equal(requests[1].max_tokens, 4096);
  assert.equal(signals[0], signals[1]);
  assert.deepEqual(requests[0].messages, requests[1].messages);
});

test('quota errors are safe and never expose provider error bodies', async () => {
  const ai = createAssistant({settings: () => ({key: 'test-secret', model: 'openrouter/free'}),
    fetchImpl: async () => new Response('provider-private-details test-secret', {status: 429})});
  await assert.rejects(ai.answer('привет'), error =>
    /Лимит/.test(error.message) && !/secret|private/.test(error.message));
});

test('recognized phrase reaches AI once and answer survives OLED delivery retry', async () => {
  let aiCalls = 0, sends = 0;
  const wav = Buffer.alloc(44); wav.write('RIFF');
  const bridge = createSpeechBridge({
    getIp: () => '192.168.0.100',
    recognizer: {ready: true, recognize: async () => 'как тебя зовут'},
    assistant: {status: () => ({configured: true}), answer: async text => {
      assert.equal(text, 'как тебя зовут'); aiCalls++; return 'Меня зовут Мочи.';
    }},
    fetchImpl: async (url, options) => {
      if (url.endsWith('/state')) return new Response(JSON.stringify({phase: 'ready', id: 123}));
      if (url.includes('/audio?')) return new Response(wav);
      assert.equal(new URLSearchParams(options.body).get('text'), 'Меня зовут Мочи.');
      if (++sends === 1) throw new Error('lost connection');
      return new Response('{}');
    },
  });
  await bridge.tick(); await bridge.tick(); await bridge.tick();
  assert.equal(aiCalls, 1);
  assert.equal(sends, 2);
  assert.equal(bridge.status.text, 'как тебя зовут');
  assert.equal(bridge.status.answer, 'Меня зовут Мочи.');
});

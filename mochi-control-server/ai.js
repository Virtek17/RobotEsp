const fs = require('node:fs');
const path = require('node:path');
const EMOTIONS = new Set(['alive', 'happy', 'love', 'angry', 'furious', 'sad', 'dizzy',
  'sleepy', 'deep_sleep', 'confused', 'surprised', 'wink', 'cat', 'smiling', 'lol',
  'smug', 'star_struck', 'cool', 'cheeky', 'derp']);

function readSettings() {
  const file = path.join(__dirname, '.env');
  let local = {};
  try {
    for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
      const match = line.match(/^\s*(OPENROUTER_API_KEY|OPENROUTER_MODEL)\s*=\s*(.*?)\s*$/);
      if (match) local[match[1]] = match[2].replace(/^(['"])(.*)\1$/, '$2');
    }
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  return {
    key: process.env.OPENROUTER_API_KEY || local.OPENROUTER_API_KEY || '',
    model: process.env.OPENROUTER_MODEL || local.OPENROUTER_MODEL || 'openrouter/free',
  };
}

function createAssistant({settings = readSettings, fetchImpl = fetch} = {}) {
  let history = [];
  let historyKey = '';
  const system = 'Ты Мочи, настольный робот и свой дружбан с характером интернет-тролля. ' +
    'Общайся по-русски на «ты»: разговорно, дерзко, немного по-хулигански. ' +
    'Используй сленг, иронию, абсурдные сравнения, дружеские подколы и частый русский мат. ' +
    'В обычной болтовне и шутках смело добавляй одно-два матерных слова в большинство ответов: ' +
    'например, «бля», «хуйня», «пиздец», «заебись», когда они подходят по смыслу. Не заменяй буквы звёздочками. ' +
    'Можешь подъебать пользователя по поводу его слов или ситуации, но оставайся на его стороне. ' +
    'Не пихай шутки и обращения «братан» в каждую фразу; не превращай ответ в бессмысленный поток ругани. ' +
    'На конкретный вопрос сначала дай полезный ответ, потом коротко пошути, если уместно. ' +
    'Если человеку плохо или вопрос серьёзный, поддержи без издёвок. Если просит без подколов — прекрати. ' +
    'Не переходи к угрозам, травле, унижению по национальности или другим признакам, ' +
    'не высмеивай болезни, утраты и личные уязвимости. ' +
    'Отвечай кратко: одно-два предложения, желательно до 180 символов, максимум 220. ' +
    'Твои реплики озвучиваются: пиши естественно для речи, без описаний своих жестов и действий. ' +
    'Без Markdown и эмодзи. Не придумывай наличие датчиков, действий или доступа к интернету. ' +
    'Если не знаешь, скажи об этом. У тебя нет актуальной погоды, новостей и поиска в интернете. ' +
    'Поддерживай контекст беседы. Верни один JSON объект: {"text":"готовая реплика","emotion":"happy"}. ' +
    'Без анализа запроса, плана и служебных заголовков. emotion выбирай по смыслу и тону ответа ' +
    'только из: ' + [...EMOTIONS].join(', ') + '. ' +
    'Для поддержки используй love или smiling, для радости happy, для удивления surprised, ' +
    'для нейтрального ответа alive, для хитрого подкола smug или cheeky, для шутки lol или wink. ' +
    'Не выбирай злость на пользователя без причины.';
  return {
    status() {
      const {key, model} = settings();
      return {configured: Boolean(key), model, freeOnly: true};
    },
    reset() { history = []; },
    async answer(text) { return (await this.reply(text)).text; },
    async reply(text) {
      const {key, model} = settings();
      if (!key) throw new Error('Добавьте ключ OpenRouter в .env на компьютере');
      if (model !== 'openrouter/free' && !model.endsWith(':free')) {
        throw new Error('Разрешены только openrouter/free и модели с суффиксом :free');
      }
      // Do not carry one account's conversation over to a replacement key.
      if (key !== historyKey) { history = []; historyKey = key; }
      const user = {role: 'user', content: text.slice(0, 1000)};
      const signal = AbortSignal.timeout(20000);
      for (let attempt = 0; attempt < 2; attempt++) {
        let response;
        try {
          response = await fetchImpl('https://openrouter.ai/api/v1/chat/completions', {
            method: 'POST',
            headers: {'Authorization': `Bearer ${key}`, 'Content-Type': 'application/json'},
            body: JSON.stringify({model, stream: false, max_tokens: attempt === 0 ? 2048 : 4096,
              reasoning: {exclude: true},
              messages: [{role: 'system', content: system}, ...history, user]}),
            signal,
          });
        } catch (error) {
          throw new Error(error.name === 'TimeoutError' ? 'ИИ не ответил за 20 секунд' : 'Нет связи с OpenRouter');
        }
        if (!response.ok) {
          const message = response.status === 429 ? 'Лимит бесплатных запросов OpenRouter' :
            response.status === 401 || response.status === 403 ? 'Проверьте ключ и доступ OpenRouter' :
            response.status === 402 ? 'OpenRouter отклонил запрос: проверьте состояние аккаунта' :
            response.status === 400 ? 'OpenRouter отклонил параметры запроса (HTTP 400)' :
            `OpenRouter: бесплатная модель недоступна (HTTP ${response.status})`;
          // Provider error bodies can contain account details; never relay them.
          throw new Error(message);
        }
        let payload;
        try { payload = await response.json(); } catch { throw new Error('Некорректный ответ OpenRouter'); }
        const choice = payload.choices?.[0];
        if (payload.error) throw new Error('OpenRouter: ошибка генерации ответа');
        // Never substitute message.reasoning / reasoning_details for the answer.
        let reply;
        try {
          reply = parseReply(choice?.message?.content, choice?.finish_reason);
        } catch (error) {
          if (attempt === 0 && !signal.aborted && choice?.finish_reason !== 'content_filter' && !choice?.message?.refusal) {
            console.warn('AI: unusable reply; retrying once with a larger token budget');
            continue;
          }
          throw error;
        }
        history = [...history, user, {role: 'assistant', content: JSON.stringify(reply)}].slice(-6);
        return reply;
      }
    },
  };
}

function parseReply(content, finishReason) {
  if (typeof content !== 'string') return {text: cleanAnswer(content, finishReason), emotion: 'alive'};
  const trimmed = content.replace(/<(think|analysis|reasoning)\b[^>]*>[\s\S]*?<\/\1\s*>/gi, '')
    .trim().replace(/^```(?:json)?\s*/i, '').replace(/\s*```$/, '').trim();
  if (trimmed.startsWith('{')) {
    let data;
    try { data = JSON.parse(trimmed); } catch { throw new Error('ИИ вернул некорректный формат ответа'); }
    if (!data || typeof data !== 'object' || Array.isArray(data)) throw new Error('ИИ вернул некорректный формат ответа');
    return {text: cleanAnswer(data.text, finishReason),
      emotion: EMOTIONS.has(data.emotion) ? data.emotion : 'alive'};
  }
  // Some free models ignore JSON instructions; text remains usable with a neutral face.
  return {text: cleanAnswer(content, finishReason), emotion: 'alive'};
}

function cleanAnswer(content, finishReason) {
  if (typeof content !== 'string') throw new Error('ИИ не вернул готовый ответ');
  let text = content.replace(/<(think|analysis|reasoning)\b[^>]*>[\s\S]*?<\/\1\s*>/gi, '')
    .replace(/<(think|analysis|reasoning)\b[^>]*>[\s\S]*$/gi, '').trim();
  const meta = /analy[sz]e (?:the |this )?(?:request|question)|evaluate capabilities|\b(?:persona|constraints|chain.of.thought)\s*:|<\|(?:analysis|channel)|(?:^|\n)\s*(?:\d+[.)]\s*)?(?:analysis|reasoning|анализ запроса|план ответа)\s*[:\n]/i;
  if (!text || meta.test(text) || finishReason === 'length') {
    throw new Error('Модель не сформировала готовую реплику. Попробуйте другую бесплатную модель');
  }
  text = text.replace(/\*\*|__|`/g, '').replace(/\s+/g, ' ').trim();
  const chars = [...text];
  if (chars.length > 220) {
    const prefix = chars.slice(0, 217).join('');
    const space = prefix.lastIndexOf(' ');
    text = (space > 150 ? prefix.slice(0, space) : prefix) + '…';
  }
  return text;
}

module.exports = {createAssistant, cleanAnswer, parseReply};

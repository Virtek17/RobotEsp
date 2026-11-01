#pragma once

// Pull-based transport: only the PC connects to the board; no PC LAN listener.
namespace Speech {
enum class Phase { Idle, Recording, Ready, Text };
Phase phase = Phase::Idle;
constexpr size_t capacity = 16000 * 4;
size_t allocatedSamples = 0;
int16_t *pcm = nullptr;
size_t used = 0;
uint32_t id = 0, startedAt = 0, readyAt = 0, textAt = 0;
String progress = "recognizing";
bool speaking = false;
bool errorText = false;
uint32_t speakingAt = 0;

void releaseAudio() {
  free(pcm);
  pcm = nullptr;
  used = 0;
  allocatedSamples = 0;
}

void showText(const String &text, bool isError = false) {
  speaking = false;
  errorText = isError;
  releaseAudio();
  // Retain the result/progress API, but never render text on the OLED.
  if (isError) Serial.println(text);
  phase = Phase::Text;
  textAt = millis();
}

bool start(bool micReady) {
  if (phase == Phase::Recording || phase == Phase::Ready) {
    Serial.println("STT: busy, still processing previous recording");
    return false;
  }
  if (!micReady) { showText("Микрофон не готов", true); return false; }
  releaseAudio();
  for (size_t seconds = 4; seconds >= 2 && !pcm; --seconds) {
    allocatedSamples = seconds * 16000;
    pcm = static_cast<int16_t *>(malloc(allocatedSamples * sizeof(int16_t)));
  }
  if (!pcm) {
    Serial.printf("STT: allocation failed, heap=%u max-block=%u\n", unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
    showText("Не хватает памяти для записи", true); return false;
  }
  ++id;
  used = 0;
  startedAt = millis();
  phase = Phase::Recording;
  speaking = false;
  errorText = false;
  Serial.printf("STT: recording (maximum %u seconds)\n", unsigned(allocatedSamples / 16000));
  return true;
}

void finish() {
  if (phase != Phase::Recording) return;
  if (used < 3200) { showText("Слишком коротко. Удерживайте кнопку и говорите", true); return; }
  phase = Phase::Ready;
  progress = "recognizing";
  readyAt = millis();
  Serial.printf("STT: phrase %lu ready, %u samples\n", (unsigned long)id, unsigned(used));
}

void sample(int32_t sample16) {
  if (phase != Phase::Recording) return;
  // Modest fixed gain for the INMP441; saturate rather than wrap.
  const int32_t amplified = sample16 * 4;
  pcm[used++] = int16_t(constrain(amplified, -32768, 32767));
  if (used == allocatedSamples) finish();
}

bool busy() { return phase == Phase::Recording || phase == Phase::Ready; }

void update() {
  if (phase == Phase::Recording && millis() - startedAt >= 4500) finish();
  if (phase == Phase::Ready && millis() - readyAt >= 45000) {
    showText("Нет ответа ПК. Запустите сервер распознавания", true);
  }
  if (speaking && millis() - speakingAt > 45000) speaking = false;
  if (phase == Phase::Text && !speaking && millis() - textAt > 10000UL) {
    phase = Phase::Idle;
  }
}

void stateFace(KitiotFace::Emotion emotion) {
  KitiotFace::updateIdleAnimations();
  KitiotFace::drawFace(emotion, false);
}

bool draw() {
  if (phase == Phase::Idle) {
    if (WiFi.status() == WL_CONNECTED) return false;
    stateFace(KitiotFace::EMO_CONFUSED);
    display.sendBuffer();
    return true;
  }
  if (phase == Phase::Recording) {
    stateFace(KitiotFace::EMO_ALIVE);
  } else if (phase == Phase::Ready) {
    stateFace(progress == "thinking" ? KitiotFace::EMO_CONFUSED : KitiotFace::EMO_ALIVE);
  } else {
    stateFace(errorText ? KitiotFace::EMO_CONFUSED : KitiotFace::activeEmotion);
  }
  display.sendBuffer();
  return true;
}

void put16(uint8_t *p, uint16_t value) { p[0] = value; p[1] = value >> 8; }
void put32(uint8_t *p, uint32_t value) {
  for (int i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}

void routes() {
  id = esp_random();
  server.on("/api/stt/state", HTTP_GET, []() {
    const char *name = phase == Phase::Recording ? "recording" :
                       phase == Phase::Ready ? "ready" : phase == Phase::Text ? "text" : "idle";
    server.send(200, "application/json", String("{\"phase\":\"") + name +
                "\",\"id\":" + String(id) + ",\"samples\":" + String(used) +
                ",\"touch\":" + (touchStable ? "true" : "false") + "}");
  });
  server.on("/api/stt/progress", HTTP_POST, []() {
    if (server.arg("id") != String(id)) {
      server.send(409, "application/json", "{\"error\":\"Recording expired\"}"); return;
    }
    const String stage = server.arg("stage");
    if (phase == Phase::Ready && (stage == "recognizing" || stage == "thinking")) {
      progress = stage;
    } else if (phase == Phase::Text && (stage == "speaking" || stage == "answered")) {
      speaking = stage == "speaking";
      speakingAt = millis();
      if (!speaking) textAt = millis();
    } else { server.send(409, "application/json", "{\"error\":\"Wrong phase\"}"); return; }
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/stt/audio", HTTP_GET, []() {
    if (phase != Phase::Ready || server.arg("id") != String(id)) {
      server.send(409, "application/json", "{\"error\":\"No matching recording\"}"); return;
    }
    uint8_t header[44] = {};
    memcpy(header, "RIFF", 4); put32(header + 4, 36 + used * 2);
    memcpy(header + 8, "WAVEfmt ", 8); put32(header + 16, 16);
    put16(header + 20, 1); put16(header + 22, 1); put32(header + 24, 16000);
    put32(header + 28, 32000); put16(header + 32, 2); put16(header + 34, 16);
    memcpy(header + 36, "data", 4); put32(header + 40, used * 2);
    server.setContentLength(sizeof(header) + used * 2);
    server.send(200, "audio/wav", "");
    server.sendContent(reinterpret_cast<const char *>(header), sizeof(header));
    server.sendContent(reinterpret_cast<const char *>(pcm), used * 2);
  });
  server.on("/api/stt/result", HTTP_POST, []() {
    if (phase != Phase::Ready || server.arg("id") != String(id)) {
      server.send(409, "application/json", "{\"error\":\"Recording expired\"}"); return;
    }
    const String text = server.arg("text");
    if (text.length() > 512) { server.send(400, "application/json", "{\"error\":\"Text too long\"}"); return; }
    const String emotion = server.arg("emotion");
    for (size_t i = 0; i < emotionCount; ++i) {
      if (emotion == emotionNames[i]) { chooseEmotion(i); break; }
    }
    showText(text.length() ? text : String("Речь не распознана"), server.arg("error") == "1");
    server.send(200, "application/json", "{\"ok\":true}");
  });
}
}  // namespace Speech

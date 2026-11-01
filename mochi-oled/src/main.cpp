#include <Arduino.h>
#include <Wire.h>

#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP_I2S.h>
#include <math.h>
#include "wifi_config.h"
#include "input_config.h"

// Default wiring shown by The Mochi. Change to match your actual connections.
constexpr int OLED_SDA = 20;
constexpr int OLED_SCL = 21;
constexpr uint8_t OLED_ADDRESS = 0x3C;

const char *const emotionNames[] = {
    "alive", "happy", "love", "angry", "furious", "sad", "dizzy",
    "sleepy", "deep_sleep", "confused", "surprised", "wink", "cat",
    "smiling", "lol", "smug", "star_struck", "cool", "cheeky", "derp"};
constexpr size_t emotionCount = sizeof(emotionNames) / sizeof(emotionNames[0]);
const char *currentEmotion = "alive";
WebServer server(80);
bool serverStarted = false;
uint32_t nextWifiLogAt = 0;


const char *wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "connected";
    case WL_NO_SSID_AVAIL: return "network not found (check SSID and 2.4 GHz)";
    case WL_CONNECT_FAILED: return "connection failed (check password and router)";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED: return "disconnected / connecting";
    case WL_IDLE_STATUS: return "connecting";
    default: return "not connected";
  }
}

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#include "kitiot_face.h"
const KitiotFace::Emotion emotionValues[] = {
    KitiotFace::EMO_ALIVE, KitiotFace::EMO_HAPPY, KitiotFace::EMO_LOVE,
    KitiotFace::EMO_ANGRY, KitiotFace::EMO_FURIOUS, KitiotFace::EMO_SAD,
    KitiotFace::EMO_DIZZY, KitiotFace::EMO_SLEEPY, KitiotFace::EMO_DEEP_SLEEP,
    KitiotFace::EMO_CONFUSED, KitiotFace::EMO_SURPRISED, KitiotFace::EMO_WINK,
    KitiotFace::EMO_CAT, KitiotFace::EMO_SMILE, KitiotFace::EMO_LAUGH,
    KitiotFace::EMO_SMUG, KitiotFace::EMO_STAR_EYES, KitiotFace::EMO_COOL,
    KitiotFace::EMO_CHEEKY, KitiotFace::EMO_DERP};
static_assert(sizeof(emotionValues) / sizeof(emotionValues[0]) == emotionCount,
              "Every API emotion must map to a face");
uint32_t nextFaceAt = 0;
size_t selectedEmotionIndex = 0;
bool soundReactionActive = false;
uint32_t soundReactionUntil = 0;
uint32_t lastSoundAt = 0;

I2SClass microphone;
bool microphoneReady = false;
uint32_t micStartedAt = 0;
uint32_t nextMicLogAt = 0;
uint32_t lastMicDataAt = 0;
float noiseRms = 100.0f;
float micRms = 0;
double micSum = 0;
double micSumSquares = 0;
double micRightSum = 0;
double micRightSumSquares = 0;
size_t micSamples = 0;
float micLeftMax = 0;
float micRightMax = 0;
uint32_t micFramesLogged = 0;
uint32_t micLeftNonzero = 0;
uint32_t micRightNonzero = 0;
esp_err_t micLastReadResult = ESP_OK;
bool touchRaw = false;
bool touchStable = false;
uint32_t touchChangedAt = 0;

void setFace(size_t index) {
  KitiotFace::activeEmotion = emotionValues[index];
  KitiotFace::isBlinking = true;
  KitiotFace::blinkStartTime = millis();
  currentEmotion = emotionNames[index];
}

void chooseEmotion(size_t index) {
  selectedEmotionIndex = index;
  soundReactionActive = false;
  setFace(index);
}

#include "speech.h"
uint32_t touchPressedAt = 0;
bool touchHeld = false;

void updateTouch() {
  const uint32_t now = millis();
  const bool touched = digitalRead(TOUCH_PIN) == (TOUCH_ACTIVE_HIGH ? HIGH : LOW);
  if (touched != touchRaw) {
    touchRaw = touched;
    touchChangedAt = now;
  }
  if (now - touchChangedAt >= 35 && touchStable != touchRaw) {
    touchStable = touchRaw;
    if (touchStable) {
      Serial.println("Touch: pressed");
      touchPressedAt = now;
      touchHeld = false;
    } else if (touchHeld) {
      Serial.println("Touch: released after hold");
      Speech::finish();
    } else if (!Speech::busy()) {
      Speech::phase = Speech::Phase::Idle;
      chooseEmotion((selectedEmotionIndex + 1) % emotionCount);
      Serial.print("Touch -> ");
      Serial.println(currentEmotion);
    }
  }
  if (touchStable && !touchHeld && now - touchPressedAt >= 350) {
    touchHeld = true;
    soundReactionActive = false;
    Speech::start(microphoneReady);
  }
}

void beginInputs() {
  pinMode(TOUCH_PIN, TOUCH_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  touchRaw = touchStable =
      digitalRead(TOUCH_PIN) == (TOUCH_ACTIVE_HIGH ? HIGH : LOW);
  Serial.println("Touch: GPIO3 | tap=emotion, hold=recording");
  if (!MICROPHONE_ENABLED) return;
  pinMode(MIC_SD_PIN, INPUT_PULLDOWN);
  microphone.setPins(MIC_SCK_PIN, MIC_WS_PIN, -1, MIC_SD_PIN);
  // Two 32-bit slots provide the 64 BCLK cycles per WS required by INMP441.
  // L/R must be grounded; only the left sample of each stereo pair is used.
  microphoneReady = microphone.begin(I2S_MODE_STD, 16000,
                                    I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  micStartedAt = lastMicDataAt = millis();
  Serial.println(microphoneReady ? "INMP441: I2S ready; keep quiet for 1.5 s"
                                : "INMP441: initialization failed; touch and face still work");
}

void updateMicrophone() {
  const uint32_t now = millis();
  if (soundReactionActive && int32_t(now - soundReactionUntil) >= 0) {
    soundReactionActive = false;
    setFace(selectedEmotionIndex);
  }
  if (!microphoneReady) return;
  int32_t stereoSamples[256];
  // Drain a bounded amount of queued audio without waiting for new samples.
  // HTTP, touch and display keep running even with a disconnected microphone.
  for (int block = 0; block < 12; ++block) {
    size_t bytesRead = 0;
    const esp_err_t result = i2s_channel_read(microphone.rxChan(), stereoSamples,
                                            sizeof(stereoSamples), &bytesRead, 0);
    micLastReadResult = result;
    if (result != ESP_OK && result != ESP_ERR_TIMEOUT) break;
    if (bytesRead == 0) break;
    lastMicDataAt = now;
    const size_t words = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i + 1 < words; i += 2) {
      const double sample = stereoSamples[i] >> 16;
      Speech::sample(stereoSamples[i] >> 16);
      const double right = stereoSamples[i + 1] >> 16;
      ++micFramesLogged;
      if ((stereoSamples[i] >> 8) != 0) ++micLeftNonzero;
      if ((stereoSamples[i + 1] >> 8) != 0) ++micRightNonzero;
      micSum += sample;
      micSumSquares += sample * sample;
      micRightSum += right;
      micRightSumSquares += right * right;
      if (++micSamples == 256) {
        const double mean = micSum / micSamples;
        const double variance = micSumSquares / micSamples - mean * mean;
        micRms = sqrt(variance > 0 ? variance : 0);
        const double rightMean = micRightSum / micSamples;
        const double rightVariance = micRightSumSquares / micSamples - rightMean * rightMean;
        const float rightRms = sqrt(rightVariance > 0 ? rightVariance : 0);
        micLeftMax = fmaxf(micLeftMax, micRms);
        micRightMax = fmaxf(micRightMax, rightRms);
        micSum = micSumSquares = 0;
        micRightSum = micRightSumSquares = 0;
        micSamples = 0;
        const bool warmingUp = now - micStartedAt < 1500;
        const float threshold = fmaxf(MIC_MIN_TRIGGER_RMS,
                                     noiseRms * MIC_NOISE_MULTIPLIER);
        if (SOUND_REACTIONS_ENABLED && Speech::phase == Speech::Phase::Idle && !warmingUp && micRms > threshold &&
            now - lastSoundAt >= SOUND_COOLDOWN_MS) {
          lastSoundAt = now;
          soundReactionActive = true;
          soundReactionUntil = now + SOUND_REACTION_MS;
          for (size_t j = 0; j < emotionCount; ++j) {
            if (emotionValues[j] == KitiotFace::EMO_SURPRISED) {
              setFace(j);
              break;
            }
          }
          Serial.println("Sound -> surprised");
        }
        if (warmingUp || micRms < threshold) {
          noiseRms += (micRms - noiseRms) * (warmingUp ? 0.1f : 0.01f);
        }
      }
    }
  }
  if (int32_t(now - nextMicLogAt) >= 0) {
    nextMicLogAt = now + 1000;
    Serial.printf("Mic RMS: %.1f | L max: %.1f | R max: %.1f | frames: %lu | nonzero L/R: %lu/%lu | trigger: %.0f%s\n",
                  micRms, micLeftMax, micRightMax,
                  (unsigned long)micFramesLogged, (unsigned long)micLeftNonzero,
                  (unsigned long)micRightNonzero,
                  fmaxf(MIC_MIN_TRIGGER_RMS, noiseRms * MIC_NOISE_MULTIPLIER),
                  now - lastMicDataAt > 1000 ? " | no I2S data" : "");
    if (micFramesLogged == 0) {
      Serial.printf("Mic: no samples; last read: %s\n", esp_err_to_name(micLastReadResult));
    } else if (micLeftNonzero == 0 && micRightNonzero == 0) {
      Serial.println("Mic: both channels are zero; check VDD=3.3V, GND, SD=4, WS=5, SCK=6");
    } else if (micLeftNonzero == 0 && micRightMax > 1) {
      Serial.println("Mic: signal is in RIGHT channel; connect L/R to GND for LEFT");
    }
    micLeftMax = micRightMax = 0;
    micFramesLogged = micLeftNonzero = micRightNonzero = 0;
  }
}

void listEmotions() {
  String result = F("{\"current\":\"");
  result += currentEmotion;
  result += F("\",\"emotions\":[");
  for (size_t i = 0; i < emotionCount; ++i) {
    if (i) result += ',';
    result += '"';
    result += emotionNames[i];
    result += '"';
  }
  result += F("]}");
  server.send(200, "application/json; charset=utf-8", result);
}

void selectEmotion() {
  String requested = server.arg("emotion");
  for (size_t i = 0; i < emotionCount; ++i) {
    if (requested == emotionNames[i]) {
      chooseEmotion(i);
      server.send(200, "application/json; charset=utf-8",
                  String("{\"current\":\"") + currentEmotion + "\"}");
      return;
    }
  }
  server.send(400, "application/json; charset=utf-8",
              "{\"error\":\"Unknown emotion\"}");
}

void setup() {
  Serial.begin(115200);
  // USB debug output must never stall HTTP/audio when no monitor drains it.
  Serial.setTxTimeoutMs(0);
  // Do not wait for a monitor: the robot must also start on USB power alone.
  Serial.println("\nMochi starting");
  display.setI2CAddress(OLED_ADDRESS << 1);
  display.begin();
  KitiotFace::activeEmotion = KitiotFace::EMO_ALIVE;
  beginInputs();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to Wi-Fi (2.4 GHz required)");
  server.on("/api/emotions", HTTP_GET, listEmotions);
  server.on("/api/emotion", HTTP_POST, selectEmotion);
  Speech::routes();
}

void loop() {
  updateTouch();
  updateMicrophone();
  Speech::update();
  // Repeat so opening Serial Monitor after boot still reveals the address.
  if (int32_t(millis() - nextWifiLogAt) >= 0) {
    nextWifiLogAt = millis() + 5000;
    Serial.print("Wi-Fi: ");
    Serial.print(wifiStatusName(WiFi.status()));
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(" | Mochi IP: ");
      Serial.print(WiFi.localIP());
    }
    Serial.println();
    Serial.printf("Robot: http=%s | button=%d/%d | speech=%d | heap=%u | max-block=%u\n",
                  serverStarted ? "started" : "stopped", int(touchRaw), int(touchStable),
                  int(Speech::phase), unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
  }
  if (WiFi.status() == WL_CONNECTED && !serverStarted) {
    Serial.print("Connected! Mochi IP: ");
    Serial.println(WiFi.localIP());
    server.begin();
    serverStarted = true;
    Serial.println("HTTP: listening on port 80");
  } else if (WiFi.status() != WL_CONNECTED && serverStarted) {
    server.stop();
    serverStarted = false;
  }
  if (serverStarted) server.handleClient();
  if (int32_t(millis() - nextFaceAt) >= 0) {
    nextFaceAt = millis() + 33;
    if (Speech::draw()) return;
    KitiotFace::updateIdleAnimations();
    KitiotFace::drawFace(KitiotFace::activeEmotion);
  }
}


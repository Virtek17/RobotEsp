#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

// INMP441 only. No OLED, touch sensor or Wi-Fi.
constexpr int SD_PIN = 4;
constexpr int WS_PIN = 5;
constexpr int SCK_PIN = 6;
I2SClass microphone;
bool ready = false;
uint32_t reportAt = 0;
uint32_t frames = 0;
uint32_t nonzero[2] = {};
int32_t minimum[2] = {INT32_MAX, INT32_MAX};
int32_t maximum[2] = {INT32_MIN, INT32_MIN};
double sum[2] = {};
double squares[2] = {};
esp_err_t lastRead = ESP_OK;

void resetStats() {
  frames = 0;
  for (int c = 0; c < 2; ++c) {
    nonzero[c] = 0;
    minimum[c] = INT32_MAX;
    maximum[c] = INT32_MIN;
    sum[c] = squares[c] = 0;
  }
}

void setup() {
  Serial.begin(115200);
  // Give USB a moment, but do not wait indefinitely for a monitor.
  delay(1000);
  Serial.println("\nINMP441 standalone test | SD=4 WS=5 SCK=6 | L/R=GND | VDD=3.3V");
  pinMode(SD_PIN, INPUT_PULLDOWN);
  microphone.setPins(SCK_PIN, WS_PIN, -1, SD_PIN);
  // Philips I2S, 16 kHz, two 32-bit slots = 64 clocks per stereo frame.
  ready = microphone.begin(I2S_MODE_STD, 16000,
                           I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  Serial.println(ready ? "I2S initialized (this does not prove the mic is connected)"
                       : "ERROR: I2S initialization failed");
  reportAt = millis() + 1000;
}

void loop() {
  if (!ready) {
    Serial.printf("I2S init error: %s\n", esp_err_to_name(microphone.lastError()));
    delay(1000);
    return;
  }
  int32_t data[256];
  size_t bytesRead = 0;
  // Short blocking read is intentional in this isolated test.
  lastRead = i2s_channel_read(microphone.rxChan(), data, sizeof(data), &bytesRead, 20);
  if (lastRead == ESP_OK || lastRead == ESP_ERR_TIMEOUT) {
    const size_t pairs = bytesRead / (2 * sizeof(int32_t));
    for (size_t i = 0; i < pairs; ++i) {
      ++frames;
      for (int c = 0; c < 2; ++c) {
        // Inspect the full signed 24-bit sample, without dropping quiet signals.
        const int32_t sample = data[i * 2 + c] >> 8;
        if (sample != 0) ++nonzero[c];
        if (sample < minimum[c]) minimum[c] = sample;
        if (sample > maximum[c]) maximum[c] = sample;
        sum[c] += sample;
        squares[c] += double(sample) * sample;
      }
    }
  }
  if (int32_t(millis() - reportAt) < 0) return;
  reportAt = millis() + 1000;
  Serial.printf("Frames: %lu | read: %s\n", (unsigned long)frames, esp_err_to_name(lastRead));
  if (frames == 0) {
    Serial.println("NO SAMPLES: I2S read failed or timed out");
  } else {
    for (int c = 0; c < 2; ++c) {
      const double mean = sum[c] / frames;
      const double variance = squares[c] / frames - mean * mean;
      const double rms = sqrt(variance > 0 ? variance : 0);
      Serial.printf("%c: RMS24=%.1f | min=%ld max=%ld | nonzero=%lu\n",
                    c == 0 ? 'L' : 'R', rms, (long)minimum[c],
                    (long)maximum[c], (unsigned long)nonzero[c]);
    }
    if (nonzero[0] == 0 && nonzero[1] == 0) {
      Serial.println("ZERO DATA: check power, wiring and solder joints");
    } else {
      Serial.println("Data present: compare RMS in silence and while speaking");
    }
  }
  resetStats();
}

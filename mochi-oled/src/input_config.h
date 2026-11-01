#pragma once
#include <stdint.h>

// ESP32-C3 SuperMini. OLED stays on SDA=20, SCL=21.
constexpr int TOUCH_PIN = 3;
constexpr bool TOUCH_ACTIVE_HIGH = true;  // Typical TTP223 module, momentary mode.
constexpr int MIC_SD_PIN = 4;
constexpr int MIC_WS_PIN = 5;
constexpr int MIC_SCK_PIN = 6;
constexpr bool MICROPHONE_ENABLED = true;
// In conversation mode, ambient sounds must not keep changing the idle face.
// Recording and AI-selected emotions do not depend on this switch.
constexpr bool SOUND_REACTIONS_ENABLED = false;

// RMS in signed 16-bit sample units, not calibrated decibels.
// Standalone RMS24 / 256 approximates this 16-bit scale.
// Observed RMS24: ~1600 quiet, ~30500 louder sound; tune further on hardware.
constexpr float MIC_MIN_TRIGGER_RMS = 80.0f;
constexpr float MIC_NOISE_MULTIPLIER = 3.5f;
constexpr uint32_t SOUND_REACTION_MS = 1500;
constexpr uint32_t SOUND_COOLDOWN_MS = 2500;

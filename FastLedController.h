#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>

#include "printer_status.h"

class FastLedController {
public:
  FastLedController();
  ~FastLedController();

  void begin();
  void update(const PrinterStatus& status);
  void reloadConfig();
  void setBrightnessPercent(uint8_t percent);
  void startPreview(String colorHex, uint8_t brightnessPercent, uint16_t breath_period_ms, uint8_t breath_amp_percent, uint8_t tempColorZone, uint32_t duration_ms);
  void setSolidColor(const CRGB& color);
  CRGB parseColor(const String& hex);

private:
  enum PrinterStateKind {
    STATE_OFFLINE = 0,
    STATE_ERROR,
    STATE_PRINTING,
    STATE_PAUSED,
    STATE_COMPLETE,
    STATE_STANDBY
  };

  void loadConfig();
  void initLeds();

  String toLower(const char* s) const;
  CRGB colorForState(PrinterStateKind kind) const;
  PrinterStateKind classifyState(const char* state) const;
  uint8_t bedTempToPaletteIndex(float temp) const;
  uint8_t breath8(uint16_t breath_period_ms) const;
  bool isState(const char* state, const char* expected) const;
  uint16_t mapLedIndex(uint16_t i) const;
  uint8_t calcBreathFactor(uint16_t periodMs, uint8_t amp) const;
  void updateRainbowState(uint16_t N, bool useRainbow, int32_t& rainbowPos, uint8_t& rainbowHue);
  CRGB getBedTempColor(uint16_t i, uint16_t N, CRGB baseColor, float bedTemp) const;
  bool renderPreviewIfActive();

  CRGB* leds = nullptr;

  uint8_t _led_pin = 5;
  String _led_type = "WS2812B";
  String _led_type_color = "GRB";
  uint16_t _led_count = 10;
  bool _led_right = false;

  uint16_t _breath_period_ms = 2000;
  uint8_t _breath_amp_percent = 50;

  uint8_t _breath_amp;

  uint8_t _brightness_percent = 50;
  uint8_t _led_modify_brightness = 0;
  uint8_t _led_modify_reverse = 0;

  CRGB standbyColor = CRGB::White;
  CRGB printColor = CRGB::Blue;
  CRGB printColor2 = CRGB::Black;
  CRGB pauseColor = CRGB::Yellow;
  CRGB errorColor = CRGB::Red;
  CRGB offlineColor = CRGB::Red;
  CRGB completeColor = CRGB::Green;

  uint8_t _bed_temp_color_st = 0;
  uint8_t _bed_temp_color_co = 0;
  uint8_t _print_center_start = 0;
  uint8_t _pause_wave_effect = 0;
  uint8_t _error_wave_effect = 0;
  uint8_t _offline_wave_effect = 0;
  uint8_t _rainbow_offline_enabled = 0;
  uint16_t _rainbow_speed_ms = 100;
  uint8_t _complete_wave_effect = 0;
  uint8_t _tempColorZonePercent = 10;

  uint32_t _rainbow_last_update = 0;
  int32_t _rainbow_position = 0;
  int8_t _rainbow_direction = 1;
  uint8_t _rainbow_hue = 0;

  Preferences prefs;

  struct PreviewState {
    bool active = false;
    uint32_t until = 0;
    CRGB color;
    uint8_t brightness = 255;
    uint16_t breath_period_ms = 2000;
    uint8_t breath_amp = (50 * 255) / 100;
    uint8_t tempColorZone = 20;
  };

  CRGBPalette16 bedTempPalette(CRGB baseStateColor) const {
    return CRGBPalette16(

      baseStateColor, baseStateColor,    // 0-31
      CRGB(255, 255, 45), CRGB::Yellow,  // 32-63

      CRGB(255, 160, 0), CRGB(255, 110, 0),  // 64-95

      CRGB(255, 80, 0), CRGB(255, 40, 0),  // 96-127

      CRGB(255, 25, 0), CRGB(255, 10, 0),  // 128-159
      CRGB::Red, CRGB::Red,                // 160-191
      CRGB::Red, CRGB::Red,                // 192-223
      CRGB::Red, CRGB::Red                 // 224-255
    );
  }

  PreviewState led_preview;
};
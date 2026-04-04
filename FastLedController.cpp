#include "FastLedController.h"

FastLedController::FastLedController() {}
FastLedController::~FastLedController() {
  if (leds) {
    delete[] leds;
    leds = nullptr;
  }
}
//=================================================================
void FastLedController::begin() {
  loadConfig();
  initLeds();
}
//=================================================================
void FastLedController::reloadConfig() {
  loadConfig();
  initLeds();
  setBrightnessPercent(_brightness_percent);
}
//=================================================================
CRGB FastLedController::colorForState(FastLedController::PrinterStateKind kind) const {
  switch (kind) {
    case STATE_OFFLINE: return offlineColor;
    case STATE_ERROR: return errorColor;
    case STATE_PRINTING: return printColor;
    case STATE_PAUSED: return pauseColor;
    case STATE_COMPLETE: return completeColor;
    case STATE_STANDBY:
    default:
      return standbyColor;
  }
}
//=================================================================
static bool containsIgnoreCase(const char* s, const char* sub) {
  if (!s || !sub) return false;

  for (; *s; ++s) {
    const char* a = s;
    const char* b = sub;

    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      ++a;
      ++b;
    }
    if (*b == '\0') return true;
  }
  return false;
}
//=================================================================
FastLedController::PrinterStateKind FastLedController::classifyState(const char* state) const {
  if (!state) return STATE_OFFLINE;

  if (containsIgnoreCase(state, "offline")) return STATE_OFFLINE;

  if (containsIgnoreCase(state, "error") || containsIgnoreCase(state, "fail") || containsIgnoreCase(state, "cancelled")) {
    return STATE_ERROR;
  }

  if (containsIgnoreCase(state, "print") || containsIgnoreCase(state, "pnt") || containsIgnoreCase(state, "printing") || containsIgnoreCase(state, "%") || containsIgnoreCase(state, "progress")) {
    return STATE_PRINTING;
  }

  if (containsIgnoreCase(state, "pause")) return STATE_PAUSED;

  if (containsIgnoreCase(state, "complete") || containsIgnoreCase(state, "done")) {
    return STATE_COMPLETE;
  }

  if (containsIgnoreCase(state, "standby") || containsIgnoreCase(state, "idle")) {
    return STATE_STANDBY;
  }

  return STATE_OFFLINE;
}
//=================================================================
void FastLedController::startPreview(String colorHex,
                                     uint8_t brightnessPercent,
                                     uint16_t breath_period_ms,
                                     uint8_t breath_amp_percent,
                                     uint8_t tempColorZone,
                                     uint32_t duration_ms) {
  led_preview.active = true;
  led_preview.until = millis() + duration_ms;

  led_preview.color = parseColor(colorHex);
  led_preview.breath_period_ms = breath_period_ms;
  led_preview.breath_amp = (breath_amp_percent * 255) / 100;

  led_preview.tempColorZone = tempColorZone * 2;
  led_preview.brightness = map(brightnessPercent, 0, 100, 0, 255);
}
//=================================================================
void FastLedController::setBrightnessPercent(uint8_t percent) {
  _brightness_percent = constrain(percent, 0, 100);
  uint8_t b = map(_brightness_percent, 0, 100, 0, 255);
  FastLED.setBrightness(b);
  FastLED.show();
}
//=================================================================
void FastLedController::setSolidColor(const CRGB& color) {
  if (!leds) return;
  for (uint16_t i = 0; i < _led_count; ++i) {
    uint16_t idx = _led_right ? (_led_count - 1 - i) : i;
    leds[idx] = color;
  }
  FastLED.show();
}
//=================================================================
uint8_t FastLedController::breath8(uint16_t breath_period_ms) const {
  uint32_t t = millis() % breath_period_ms;
  uint8_t phase = (uint8_t)((t * 256) / breath_period_ms);
  return sin8(phase);
}
//=================================================================
uint8_t FastLedController::bedTempToPaletteIndex(float temp) const {
  if (temp < 35) return 0;                            //baseStateColor, baseStateColor,
  if (temp < 45) return map(temp, 35, 45, 32, 64);    //CRGB(255,255,46), CRGB::Yellow,
  if (temp < 55) return map(temp, 45, 55, 65, 96);    // CRGB(255,160,0), CRGB(255,110,0),
  if (temp < 65) return map(temp, 55, 65, 96, 128);   //CRGB(255,80,0), CRGB(255,40,0),   // 96-127
  if (temp < 75) return map(temp, 65, 75, 128, 160);  // CRGB(255,25,0), CRGB(255,10,0),    // 128-159
  if (temp < 85) return map(temp, 75, 85, 160, 192);  // CRGB::Red, CRGB::Red,             // 160-191
  return map(temp, 85, 200, 192, 255);                //CRGB::Red, CRGB::Red,             // 160-255
}
//=================================================================
bool FastLedController::isState(const char* state, const char* expected) const {
  if (!state || !expected) return false;
  return strcmp(state, expected) == 0;
}
//=================================================================
uint16_t FastLedController::mapLedIndex(uint16_t i) const {
  return _led_right ? (_led_count - 1 - i) : i;
}
//=================================================================
uint8_t FastLedController::calcBreathFactor(uint16_t periodMs, uint8_t amp) const {
  uint8_t breath = breath8(periodMs);
  uint8_t breathAmp = scale8_video(breath, amp);
  uint8_t baseMin = 255 - amp;
  return baseMin + breathAmp;
}
//=================================================================
void FastLedController::updateRainbowState(uint16_t N, bool useRainbow, int32_t& rainbowPos, uint8_t& rainbowHue) {
  rainbowPos = 0;
  rainbowHue = 0;

  if (!useRainbow || N == 0) return;

  uint32_t now = millis();
  if (now - _rainbow_last_update >= _rainbow_speed_ms) {
    _rainbow_last_update = now;

    if (N <= 1) {
      _rainbow_position = 0;
      _rainbow_direction = 1;
    } else {
      if (_rainbow_direction > 0) {
        if (_rainbow_position < (int32_t)(N - 1)) {
          _rainbow_position++;
        } else {
          _rainbow_direction = -1;
        }
      } else {
        if (_rainbow_position > 0) {
          _rainbow_position--;
        } else {
          _rainbow_direction = 1;
        }
      }
    }

    _rainbow_hue += 3;
  }

  rainbowPos = _rainbow_position;
  rainbowHue = _rainbow_hue;
}
//=================================================================
CRGB FastLedController::getBedTempColor(uint16_t i, uint16_t N, CRGB baseColor, float bedTemp) const {
  float center = (N - 1) * 0.5f;
  float maxDist = center;

  float tempColorZonePercent = _tempColorZonePercent * 2;
  float zoneHalf = maxDist * (tempColorZonePercent / 100.0f);
  float tempZoneStart = maxDist - zoneHalf;
  float tempZoneSize = zoneHalf;

  float dist = fabsf((float)i - center);
  if (dist <= tempZoneStart || tempZoneSize <= 0.0f) {
    return baseColor;
  }

  float x = (dist - tempZoneStart) / tempZoneSize;
  if (x > 1.0f) x = 1.0f;
  x = x * x;

  uint8_t tempIndex = bedTempToPaletteIndex(bedTemp);
  uint8_t idxTemp = (uint8_t)(tempIndex * x);

  CRGBPalette16 tempPalette = bedTempPalette(baseColor);
  return ColorFromPalette(tempPalette, idxTemp, 255, LINEARBLEND);
}
//=================================================================
bool FastLedController::renderPreviewIfActive() {
  if (!led_preview.active) return false;

  if (millis() >= led_preview.until) {
    led_preview.active = false;
    return false;
  }

  const uint16_t N = _led_count;
  uint8_t breathFactor = calcBreathFactor(led_preview.breath_period_ms, led_preview.breath_amp);
  uint8_t finalBright = scale8_video(led_preview.brightness, breathFactor);
  CRGB base = led_preview.color;

  if (led_preview.tempColorZone > 0) {
    float center = (N - 1) * 0.5f;
    float maxDist = center;
    float zoneHalf = maxDist * (led_preview.tempColorZone / 100.0f);
    float tempZoneStart = maxDist - zoneHalf;
    float tempZoneSize = zoneHalf;

    CRGBPalette16 tempPalette = bedTempPalette(base);
    uint8_t tempIndex = bedTempToPaletteIndex(120);

    for (uint16_t i = 0; i < N; ++i) {
      uint16_t idx = mapLedIndex(i);
      float dist = fabsf((float)i - center);

      CRGB tmp;
      if (dist <= tempZoneStart) {
        tmp = base;
      } else {
        float x = (dist - tempZoneStart) / tempZoneSize;
        if (x > 1.0f) x = 1.0f;
        x = x * x;

        uint8_t idxTemp = (uint8_t)(tempIndex * x);
        tmp = ColorFromPalette(tempPalette, idxTemp, 255, LINEARBLEND);
      }

      tmp.nscale8_video(finalBright);
      leds[idx] = tmp;
    }
  } else {
    fill_solid(leds, N, base);
    for (uint16_t i = 0; i < N; ++i) {
      leds[i].nscale8_video(finalBright);
    }
  }

  return true;
}
//=================================================================
void FastLedController::update(const PrinterStatus& status) {
  if (!leds) return;

  if (renderPreviewIfActive()) {
    FastLED.show();
    return;
  }

  const uint16_t N = _led_count;
  const uint32_t now = millis();

  // ---- status flags ----
  PrinterStateKind stateKind = classifyState(status.state);
  CRGB baseColor = colorForState(stateKind);

  bool isPrinting = (stateKind == STATE_PRINTING);
  bool isStandby = (stateKind == STATE_STANDBY);
  bool isComplete = (stateKind == STATE_COMPLETE);
  bool isPause = (stateKind == STATE_PAUSED);
  bool isError = (stateKind == STATE_ERROR);
  bool isOffline = (stateKind == STATE_OFFLINE);

  bool useWave =
    (isComplete && (_complete_wave_effect > 0)) || (isPause && (_pause_wave_effect > 0)) || (isError && (_error_wave_effect > 0)) || (isOffline && (_offline_wave_effect > 0));

  bool useBedTemp =
    (isStandby && (_bed_temp_color_st > 0)) || (isComplete && (_bed_temp_color_co > 0));

  bool useRainbow =
    (isOffline && (_rainbow_offline_enabled > 0));

  //wave precalc
  float wavePos = 0.0f;
  uint16_t waveWidth = 1;
  if (useWave) {
    waveWidth = max<uint16_t>(1, N / 4);
    const uint16_t cycle = 3500;

    float phase = (float)(now % cycle) / cycle;
    float posPhase = (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
    float evenFix = (N % 2 == 0) ? 0.5f : 0.0f;
    float range = N + waveWidth * 2;

    wavePos = -waveWidth - evenFix + posPhase * range;
  }

  // ---- rainbow state ----
  int32_t rainbowPos = 0;
  uint8_t rainbowHue = 0;
  // ---- rainbow tail length ----
  uint16_t rainbowLen = N / 10;
  if (rainbowLen < 2) rainbowLen = 2;
  if (rainbowLen > 6) rainbowLen = 6;
  updateRainbowState(N, useRainbow, rainbowPos, rainbowHue);

  // ---- breath ----
  uint8_t breathFactor = calcBreathFactor(_breath_period_ms, _breath_amp);

  // ---- led state modify
  if ((_led_modify_brightness > 0) && (status.ledState >= 0.0f)) {
    float s = status.ledState;
    if (s > 1.0f) s = 1.0f;
    breathFactor = scale8_video(breathFactor, (uint8_t)(s * 255.0f + 0.5f));
  }

  // ---- progress ----
  bool useProgress =
    isPrinting && (status.progress >= 0.0f) && (status.progress <= 1.0f);

  bool centerMode = (_print_center_start > 0);

  uint16_t fullLit = 0;
  uint8_t partial8 = 0;

  float center = (N - 1) * 0.5f;
  uint16_t fullSpan = 0;
  uint8_t partialSpan8 = 0;

  if (useProgress) {
    float ledProgress = status.progress * (float)N;
    fullLit = (uint16_t)ledProgress;
    if (fullLit > N) fullLit = N;

    float frac = ledProgress - (float)fullLit;
    partial8 = (uint8_t)(frac * 255.0f + 0.5f);

    if (centerMode) {
      float span = status.progress * (float)N;
      float halfSpan = span * 0.5f;
      fullSpan = (uint16_t)halfSpan;

      float fracSpan = halfSpan - (float)fullSpan;
      partialSpan8 = (uint8_t)(fracSpan * 255.0f + 0.5f);
    }
  }

  for (uint16_t i = 0; i < N; ++i) {
    uint16_t idx = mapLedIndex(i);

    uint8_t waveMul = 255;
    if (useWave) {
      float dist = (float)i - wavePos;
      if (dist < 0) dist = -dist;

      if (dist < waveWidth) {
        float x = dist / waveWidth;
        waveMul = 110 + uint8_t((1.0f - x * x) * 145);
      } else {
        waveMul = 110;
      }
    }

    uint8_t baseBrightness = 255;
    if (useProgress) {
      if (!centerMode) {
        if (i < fullLit) {
          baseBrightness = 255;
        } else if (i == fullLit && partial8 > 0) {
          baseBrightness = partial8;
        } else {
          baseBrightness = 0;
        }
      } else {
        float dist = fabsf((float)i - center);
        if (dist < fullSpan) {
          baseBrightness = 255;
        } else if (dist < fullSpan + 1 && partialSpan8 > 0) {
          baseBrightness = partialSpan8;
        } else {
          baseBrightness = 0;
        }
      }
    }

    uint8_t finalBright = scale8_video(baseBrightness, breathFactor);
    uint8_t bright = scale8_video(finalBright, waveMul);

    // ---- printing branch ----
    if (useProgress) {
      if (baseBrightness == 0) {
        CRGB off = printColor2;
        off.nscale8_video(breathFactor);
        leds[idx] = off;
      } else if (baseBrightness == 255) {
        CRGB tmp = printColor;
        tmp.nscale8_video(finalBright);
        leds[idx] = tmp;
      } else {
        CRGB blended = blend(printColor2, printColor, baseBrightness);
        blended.nscale8_video(breathFactor);
        leds[idx] = blended;
      }
      continue;
    }

    // ---- non-printing branch ----
    if (finalBright == 0) {
      leds[idx] = CRGB::Black;
      continue;
    }

    CRGB tmp = baseColor;

    if (useBedTemp) {
      tmp = getBedTempColor(i, N, baseColor, status.bedTemp);
    } else if (useRainbow) {
      tmp = baseColor;

      for (uint16_t k = 0; k < rainbowLen; ++k) {
        int32_t p;

        if (_rainbow_direction > 0) {
          // движение вправо -> хвост слева
          p = rainbowPos - (int32_t)k;
        } else {
          // движение влево -> хвост справа
          p = rainbowPos + (int32_t)k;
        }

        // сжимаем хвост в крайний пиксель
        if (p < 0) p = 0;
        if (p >= (int32_t)N) p = (int32_t)N - 1;

        if ((int32_t)i == p) {
          uint8_t val = (k == 0) ? 255 : (255 - (k * 200) / rainbowLen);
          CRGB c = CHSV(rainbowHue, 255, val);

          if (tmp.r < c.r) tmp.r = c.r;
          if (tmp.g < c.g) tmp.g = c.g;
          if (tmp.b < c.b) tmp.b = c.b;
        }
      }
    }

    tmp.nscale8_video(bright);
    leds[idx] = tmp;
  }

  FastLED.show();
}
//============================================================================
// ---------- private ----------

void FastLedController::loadConfig() {
  // led namespace
  prefs.begin("led", true);
  _led_pin = prefs.getUChar("led_pin", 4);             // default 4
  _led_type = prefs.getString("led_type", "WS2812B");  // WS2812B default
  _led_type_color = prefs.getString("led_type_color", "GRB");
  _led_count = prefs.getUChar("led_count", 16);
  _led_right = prefs.getUChar("led_right", 0) ? true : false;
  prefs.end();

  // color namespace
  prefs.begin("color", true);
  _brightness_percent = prefs.getUChar("brightness", 50);
  _led_modify_brightness = prefs.getUChar("led_modify", 0);
  standbyColor = parseColor(prefs.getString("standby_color", "#FFFFFF"));
  printColor = parseColor(prefs.getString("print_color", "#529dff"));
  printColor2 = parseColor(prefs.getString("print_color2", "#000000"));
  pauseColor = parseColor(prefs.getString("pause_color", "#ffe438"));
  errorColor = parseColor(prefs.getString("error_color", "#fe1010"));
  offlineColor = parseColor(prefs.getString("offline_color", "#101010"));
  completeColor = parseColor(prefs.getString("complete_color", "#24ff5b"));
  prefs.end();

  // breath namespace
  prefs.begin("breath", true);
  _breath_period_ms = prefs.getUShort("period_ms", 2000);   // default 2000 мс
  _breath_amp_percent = prefs.getUChar("amp_percent", 50);  // default 50%
  prefs.end();

  _breath_amp = (_breath_amp_percent * 255) / 100;

  prefs.begin("effects", true);
  _bed_temp_color_st = prefs.getUChar("bed_temp_c_st", 0);
  _bed_temp_color_co = prefs.getUChar("bed_temp_c_co", 0);
  _tempColorZonePercent = prefs.getUChar("t_color_zone", 10);
  _print_center_start = prefs.getUChar("prt_center_st", 0);
  _pause_wave_effect = prefs.getUChar("pause_wave_ef", 0);
  _error_wave_effect = prefs.getUChar("error_wave_ef", 0);
  _offline_wave_effect = prefs.getUChar("off_wave_ef", 0);
  _rainbow_offline_enabled = prefs.getUChar("off_rainbow", 1);
  _rainbow_speed_ms = prefs.getUShort("rainbow_speed", 50);
  _complete_wave_effect = prefs.getUChar("compl_wave_ef", 0);
  prefs.end();

  // sanitize
  if (_led_count == 0) _led_count = 1;
  if (_led_count > 1024) _led_count = 1024;  // safety
}
//=================================================================
CRGB FastLedController::parseColor(const String& hex) {
  if (hex.length() < 7) return CRGB::White;
  const char* cstr = hex.c_str();
  const char* p = cstr;
  if (p[0] == '#') ++p;
  char buf[7] = { 0 };
  strncpy(buf, p, 6);
  long val = strtol(buf, nullptr, 16);
  uint8_t r = (val >> 16) & 0xFF;
  uint8_t g = (val >> 8) & 0xFF;
  uint8_t b = val & 0xFF;
  return CRGB(r, g, b);
}
//=================================================================
void FastLedController::initLeds() {
  if (leds) {
    delete[] leds;
    leds = nullptr;
  }
  leds = new CRGB[_led_count];

  uint8_t b = map(_brightness_percent, 0, 100, 0, 255);
  FastLED.setBrightness(b);

  FastLED.clear(true);

  if (_led_type == "WS2812B") {
    switch (_led_pin) {
      case 4:
        if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 4, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 4, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 4, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 4, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2812B, 4, GRB>(leds, _led_count);
        break;
        case 5:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 5, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 5, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 5, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
          break;
        case 1:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 1, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 1, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 1, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 1, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812B, 1, GRB>(leds, _led_count);
          break;
        case 2:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 2, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 2, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 2, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 2, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812B, 2, GRB>(leds, _led_count);
          break;
    }
    // done WS2812B
  }
  // WS2812 (alias)
  else if (_led_type == "WS2812") {
    switch (_led_pin) {
      case 4:
        if (_led_type_color == "GRB") FastLED.addLeds<WS2812, 4, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2812, 4, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2812, 4, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2812, 4, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2812, 4, GRB>(leds, _led_count);
        break;
        case 5:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812, 5, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812, 5, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812, 5, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812, 5, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812, 5, GRB>(leds, _led_count);
          break;
        case 1:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812, 1, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812, 1, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812, 1, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812, 1, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812, 1, GRB>(leds, _led_count);
          break;
        case 2:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2812, 2, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2812, 2, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2812, 2, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2812, 2, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2812, 2, GRB>(leds, _led_count);
          break;
    }
  }
  // WS2813
  else if (_led_type == "WS2813") {
    switch (_led_pin) {
      case 4:
        if (_led_type_color == "GRB") FastLED.addLeds<WS2813, 4, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2813, 4, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2813, 4, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2813, 4, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2813, 4, GRB>(leds, _led_count);
        break;
        case 5:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2813, 5, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2813, 5, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2813, 5, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2813, 5, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2813, 5, GRB>(leds, _led_count);
          break;
        case 1:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2813, 1, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2813, 1, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2813, 1, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2813, 1, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2813, 1, GRB>(leds, _led_count);
          break;
        case 2:
          if (_led_type_color == "GRB") FastLED.addLeds<WS2813, 2, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<WS2813, 2, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<WS2813, 2, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<WS2813, 2, RBG>(leds, _led_count);
          else FastLED.addLeds<WS2813, 2, GRB>(leds, _led_count);
          break;
    }
  }
  // SK6812 (RGBW variants exist; this handles RGB SK6812)
  else if (_led_type == "SK6812") {
    switch (_led_pin) {
      case 4:
        if (_led_type_color == "GRB") FastLED.addLeds<SK6812, 4, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<SK6812, 4, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<SK6812, 4, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<SK6812, 4, RBG>(leds, _led_count);
        else FastLED.addLeds<SK6812, 4, GRB>(leds, _led_count);
        break;
        case 5:
          if (_led_type_color == "GRB") FastLED.addLeds<SK6812, 5, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<SK6812, 5, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<SK6812, 5, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<SK6812, 5, RBG>(leds, _led_count);
          else FastLED.addLeds<SK6812, 5, GRB>(leds, _led_count);
          break;
        case 1:
          if (_led_type_color == "GRB") FastLED.addLeds<SK6812, 1, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<SK6812, 1, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<SK6812, 1, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<SK6812, 1, RBG>(leds, _led_count);
          else FastLED.addLeds<SK6812, 1, GRB>(leds, _led_count);
          break;
        case 2:
          if (_led_type_color == "GRB") FastLED.addLeds<SK6812, 2, GRB>(leds, _led_count);
          else if (_led_type_color == "RGB") FastLED.addLeds<SK6812, 2, RGB>(leds, _led_count);
          else if (_led_type_color == "BGR") FastLED.addLeds<SK6812, 2, BGR>(leds, _led_count);
          else if (_led_type_color == "RBG") FastLED.addLeds<SK6812, 2, RBG>(leds, _led_count);
          else FastLED.addLeds<SK6812, 2, GRB>(leds, _led_count);
          break;
    }
  } 

  FastLED.clear(true);
  FastLED.show();
}
#include "FastLedController.h"

FastLedController::FastLedController() {}
FastLedController::~FastLedController() {
  if (leds) {
    delete[] leds;
    leds = nullptr;
  }
}

void FastLedController::begin() {
  loadConfig();
  initLeds();
}

void FastLedController::reloadConfig() {
  loadConfig();
  initLeds();
  setBrightnessPercent(_brightness_percent);
}


void FastLedController::startPreview(String colorHex,
                  uint8_t brightnessPercent,
                  uint16_t breath_period_ms, 
                  uint8_t breath_amp_percent,
                  uint8_t tempColorZone,
                  uint32_t duration_ms)
{
    led_preview.active = true;
    led_preview.until = millis() + duration_ms;

    led_preview.color = parseColor(colorHex);
    led_preview.breath_period_ms = breath_period_ms;
    led_preview.breath_amp = (breath_amp_percent * 255) / 100;

    led_preview.tempColorZone = tempColorZone*2;
    led_preview.brightness = map(brightnessPercent, 0, 100, 0, 255);
}

void FastLedController::setBrightnessPercent(uint8_t percent) {
  _brightness_percent = constrain(percent, 0, 100);
  uint8_t b = map(_brightness_percent, 0, 100, 0, 255);
  FastLED.setBrightness(b);
  FastLED.show();
}

void FastLedController::setSolidColor(const CRGB &color) {
  if (!leds) return;
  for (uint16_t i = 0; i < _led_count; ++i) {
    uint16_t idx = _led_right ? (_led_count - 1 - i) : i;
    leds[idx] = color;
  }
  FastLED.show();
}

uint8_t FastLedController::breath8(uint16_t breath_period_ms) {
    uint32_t t = millis() % breath_period_ms;
    uint8_t phase = (uint8_t)((t * 256) / breath_period_ms);
    return sin8(phase);
}

uint8_t FastLedController::bedTempToPaletteIndex(float temp)
{
    if (temp < 35) return 0; //baseStateColor, baseStateColor,
    if (temp < 45) return map(temp, 35, 45, 32, 64); //CRGB(255,255,46), CRGB::Yellow,  
    if (temp < 55) return map(temp, 45, 55, 65, 96); // CRGB(255,160,0), CRGB(255,110,0),
    if (temp < 65) return map(temp, 55, 65, 96, 128); //CRGB(255,80,0), CRGB(255,40,0),   // 96-127
    if (temp < 75) return map(temp, 65, 75, 128, 160); // CRGB(255,25,0), CRGB(255,10,0),    // 128-159
    if (temp < 85) return map(temp, 75, 85, 160, 192);// CRGB::Red, CRGB::Red,             // 160-191
    return map(temp, 85, 200, 192, 255); //CRGB::Red, CRGB::Red,             // 160-255
}

void FastLedController::update(const PrinterStatus& status) {
    if (!leds) return;

  if (!leds) return;

    if (led_preview.active) {
        if (millis() < led_preview.until) {

            const uint16_t N = _led_count;

            uint8_t breath = breath8(led_preview.breath_period_ms);
            uint8_t breathAmp = scale8_video(breath, led_preview.breath_amp);
            uint8_t baseMin = 255 - led_preview.breath_amp;
            uint8_t breathFactor = baseMin + breathAmp;

            uint8_t finalBright = scale8_video(led_preview.brightness, breathFactor);
            CRGB base = led_preview.color;

            if (led_preview.tempColorZone > 0) {

                float center = (N - 1) * 0.5f;
                float maxDist = center;
                float zoneHalf = maxDist * (led_preview.tempColorZone / 100.0f);
                float tempZoneStart = maxDist - zoneHalf;
                float tempZoneSize = zoneHalf;

                CRGBPalette16 TempPalette = bedTempPalette(base);
                uint8_t tempIndex = bedTempToPaletteIndex(120); 

                for (uint16_t i = 0; i < N; ++i) {
                    uint16_t idx = _led_right ? (N - 1 - i) : i;
                    float dist = fabsf((float)i - center);

                    CRGB tmp;
                    if (dist <= tempZoneStart) {
                        tmp = base; 
                    } else {
                        float x = (dist - tempZoneStart) / tempZoneSize;
                        if (x > 1.0f) x = 1.0f;

                        x = x * x; 
                        uint8_t idxTemp = (uint8_t)(tempIndex * x);

                        tmp = ColorFromPalette(TempPalette, idxTemp, 255, LINEARBLEND);
                    }

                    tmp.nscale8_video(finalBright);
                    leds[idx] = tmp;
                }

            } else {
                base.nscale8_video(finalBright);
                fill_solid(leds, N, base);
            }

            FastLED.show();
            return;
        } else {
            led_preview.active = false;
        }
    }

    CRGB baseColor = colorForState(status.state);
    const uint16_t N = _led_count;

    //breath
    uint8_t breath = breath8(_breath_period_ms);
    uint8_t breathAmp = scale8_video(breath, _breath_amp);
    uint8_t baseMin = 255 - _breath_amp;
    uint8_t breathFactor = baseMin + breathAmp;

    //wave
    bool isStandby = (baseColor == standbyColor);
    bool isComplete = (baseColor == completeColor);
    bool isPause = (baseColor == pauseColor);
    bool isError = (baseColor == errorColor);
    bool isOffline = (baseColor == offlineColor);
    bool useWave = (isComplete && (_complete_wave_effect > 0)) || (isPause && (_pause_wave_effect > 0)) || (isError && (_error_wave_effect > 0)) || (isOffline && (_offline_wave_effect > 0));

    bool useBedTemp = (isStandby && (_bed_temp_color_st > 0)) || (isComplete && (_bed_temp_color_co > 0));

    uint16_t waveWidth = max<uint16_t>(1, N / 4);
    uint32_t t = millis();                        
    uint16_t cycle = 3500;                      

    //progress
    bool useProgress = (status.progress >= 0.0f && status.progress <= 1.0f) &&
                       (strncmp(status.state, "printing", 8) == 0);

    uint16_t fullLit = 0;
    uint8_t partial8 = 0;

    bool centerMode = (_print_center_start > 0);

    float center = (N - 1) * 0.5f;
    float halfSpan = 0;
    uint16_t fullSpan = 0;
    uint8_t partialSpan8 = 0;

    if (useProgress && centerMode) {

        float span = status.progress * (float)N;
        halfSpan = span * 0.5f;

        fullSpan = (uint16_t)halfSpan;
        float frac = halfSpan - (float)fullSpan;
        partialSpan8 = (uint8_t)(frac * 255.0f + 0.5f);
    }

    if (useProgress) {
        float ledProgress = status.progress * (float)N;
        fullLit = (uint16_t)ledProgress;
        if (fullLit > N) fullLit = N;
        float frac = ledProgress - (float)fullLit;
        partial8 = (uint8_t)(frac * 255.0f + 0.5f);
    }

    uint8_t tempIndex = bedTempToPaletteIndex(status.bedTemp);

    float maxDist = center;
    float tempColorZonePercent = _tempColorZonePercent * 2;

    float zoneHalf = maxDist * (tempColorZonePercent / 100.0f);

    float tempZoneStart = maxDist - zoneHalf;

    float tempZoneSize = zoneHalf;
    CRGBPalette16 TempPalette = bedTempPalette(baseColor);
    
    for (uint16_t i = 0; i < N; ++i) {

        uint16_t idx = _led_right ? (N - 1 - i) : i;

        uint8_t waveMul = 255;

        if (useWave) {

            float phase = (float)(t % cycle) / cycle;

            float posPhase;
            if (phase < 0.5f) {
                posPhase = phase * 2.0f;
            } else {
                posPhase = (1.0f - phase) * 2.0f;
            }
            float evenFix = (N % 2 == 0) ? 0.5f : 0.0f;

            float range = N + waveWidth * 2;
            float wavePos = -waveWidth - evenFix + posPhase * range;

            float dist = (float)i - wavePos;
            if (dist < 0) dist = -dist;

            if (dist < waveWidth) {
                float x = dist / waveWidth;
                uint8_t waveValue = 110 + uint8_t((1.0f - x*x) * 145);
                if (waveValue > waveMul) waveMul = waveValue;
            } else {
                waveMul = 110;
            }
        }

        uint8_t baseBrightness;

        if (useProgress) {

            if (!centerMode) {
                if (i < fullLit) baseBrightness = 255;
                else if (i == fullLit && partial8 > 0) baseBrightness = partial8;
                else baseBrightness = 0;
            }
            else {
                float dist = fabsf((float)i - center);

                if (dist < fullSpan) {
                    baseBrightness = 255;
                }
                else if (dist < fullSpan + 1 && partialSpan8 > 0) {
                    baseBrightness = partialSpan8;
                }
                else {
                    baseBrightness = 0;
                }
            }

        } else {
            baseBrightness = 255;
        }

        uint8_t finalBright = scale8_video(baseBrightness, breathFactor);
        uint8_t bright = scale8_video(finalBright, waveMul);

        if (useProgress) {
            if (baseBrightness == 0) {

                CRGB off = printColor2;
                off.nscale8_video(breathFactor);
                leds[idx] = off;
            }
            else if (baseBrightness == 255) {
                CRGB tmp = printColor;
                tmp.nscale8_video(finalBright);
                leds[idx] = tmp;
            }
            else {
                CRGB blended = blend(printColor2, printColor, baseBrightness);
                blended.nscale8_video(breathFactor);
                leds[idx] = blended;
            }

        } else {
            CRGB tmp;
            if (finalBright == 0) {
                leds[idx] = CRGB::Black;
            } else {
                if (useBedTemp) {
                    float dist = fabsf((float)idx - center);
                    if (dist <= tempZoneStart) {
                        tmp = baseColor;
                    }
                    else {
                        float x = (dist - tempZoneStart) / tempZoneSize;
                        if (x > 1.0f) x = 1.0f;
                        x = x * x;  
                        uint8_t idxTemp = (uint8_t)(tempIndex * x);
                        tmp = ColorFromPalette(TempPalette, idxTemp, 255, LINEARBLEND);
                    }

                } else {
                    tmp = baseColor;
                }
                tmp.nscale8_video(bright);
                leds[idx] = tmp;
            }
        }
    }

    FastLED.show();
}
// ---------- private ----------

void FastLedController::loadConfig() {
  // led namespace
  prefs.begin("led", true);
  _led_pin = prefs.getUChar("led_pin", 4);                 // default 4
  _led_type = prefs.getString("led_type", "WS2812B");      // WS2812B default
  _led_type_color = prefs.getString("led_type_color", "GRB");
  _led_count = prefs.getUChar("led_count", 16);
  _led_right = prefs.getUChar("led_right", 0) ? true : false;
  prefs.end();

  // color namespace
  prefs.begin("color", true);
  _brightness_percent = prefs.getUChar("brightness", 50);
  standbyColor  = parseColor(prefs.getString("standby_color", "#FFFFFF"));
  printColor    = parseColor(prefs.getString("print_color",   "#529dff"));
  printColor2   = parseColor(prefs.getString("print_color2",   "#000000"));
  pauseColor    = parseColor(prefs.getString("pause_color",   "#ffe438"));
  errorColor    = parseColor(prefs.getString("error_color",   "#fe1010"));
  offlineColor  = parseColor(prefs.getString("offline_color",   "#fe1010"));
  completeColor = parseColor(prefs.getString("complete_color","#24ff5b"));
  prefs.end();

  // breath namespace
  prefs.begin("breath", true);
  _breath_period_ms = prefs.getUShort("period_ms", 2000);   // default 2000 мс
  _breath_amp_percent = prefs.getUChar("amp_percent", 50); // default 50%
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
  _complete_wave_effect = prefs.getUChar("compl_wave_ef", 0);
  prefs.end();

  // sanitize
  if (_led_count == 0) _led_count = 1;
  if (_led_count > 1024) _led_count = 1024; // safety
}

CRGB FastLedController::parseColor(const String &hex) {
  if (hex.length() < 7) return CRGB::White;
  const char *cstr = hex.c_str();
  const char *p = cstr;
  if (p[0] == '#') ++p;
  char buf[7] = {0};
  strncpy(buf, p, 6);
  long val = strtol(buf, nullptr, 16);
  uint8_t r = (val >> 16) & 0xFF;
  uint8_t g = (val >> 8) & 0xFF;
  uint8_t b = val & 0xFF;
  return CRGB(r, g, b);
}

String FastLedController::toLower(const char *s) {
  String out;
  for (size_t i = 0; s && s[i]; ++i) out += (char)tolower((unsigned char)s[i]);
  return out;
}

CRGB FastLedController::colorForState(const char *state) {
  if (!state) return CRGB::Black;
  String st = toLower(state);

  if (st.indexOf("offline") >= 0) return offlineColor;
  if (st.indexOf("error") >= 0 || st.indexOf("fail") >= 0 || st.indexOf("cancelled") >= 0) return errorColor;
  if (st.indexOf("print") >= 0 || st.indexOf("printing") >= 0 || st.indexOf("pnt") >= 0) return printColor;
  if (st.indexOf("pause") >= 0 || st.indexOf("paused") >= 0) return pauseColor;
  if (st.indexOf("complete") >= 0 || st.indexOf("done") >= 0) return completeColor;
  if (st.indexOf("standby") >= 0 || st.indexOf("idle") >= 0) return standbyColor;

  if (st.indexOf("%") >= 0 || st.indexOf("progress") >= 0) return printColor;

  return standbyColor;
}

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
      default:
        // fallback to pin 5
        if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 5, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 5, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 5, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
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
      default:
        if (_led_type_color == "GRB") FastLED.addLeds<WS2812, 5, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2812, 5, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2812, 5, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2812, 5, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2812, 5, GRB>(leds, _led_count);
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
      default:
        if (_led_type_color == "GRB") FastLED.addLeds<WS2813, 5, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<WS2813, 5, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<WS2813, 5, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<WS2813, 5, RBG>(leds, _led_count);
        else FastLED.addLeds<WS2813, 5, GRB>(leds, _led_count);
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
      default:
        if (_led_type_color == "GRB") FastLED.addLeds<SK6812, 5, GRB>(leds, _led_count);
        else if (_led_type_color == "RGB") FastLED.addLeds<SK6812, 5, RGB>(leds, _led_count);
        else if (_led_type_color == "BGR") FastLED.addLeds<SK6812, 5, BGR>(leds, _led_count);
        else if (_led_type_color == "RBG") FastLED.addLeds<SK6812, 5, RBG>(leds, _led_count);
        else FastLED.addLeds<SK6812, 5, GRB>(leds, _led_count);
        break;
    }
  }
  else {
    // fallback to WS2812B on pin 5
    if (_led_type_color == "GRB") FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
    else if (_led_type_color == "RGB") FastLED.addLeds<WS2812B, 5, RGB>(leds, _led_count);
    else if (_led_type_color == "BGR") FastLED.addLeds<WS2812B, 5, BGR>(leds, _led_count);
    else if (_led_type_color == "RBG") FastLED.addLeds<WS2812B, 5, RBG>(leds, _led_count);
    else FastLED.addLeds<WS2812B, 5, GRB>(leds, _led_count);
  }

  FastLED.clear(true);
  FastLED.show();
}
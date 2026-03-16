#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;

static const char* AP_SSID = "Klipper_State_LED_Config";
static const char* AP_PASS = "12345678";

static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t APPLY_AFTER_SUCC_MS = 3000;

static volatile WifiState state = WIFI_IDLE;
static unsigned long connectStartMs = 0;

static String tempSSID;
static String tempPASS;
static bool tryFromPrefs = false;

static bool pendingApply = false;
static unsigned long pendingApplyAt = 0;

static void startSTA(const String& ssid, const String& pass) {
  WiFi.disconnect(true);
  delay(20);
  WiFi.begin(ssid.c_str(), pass.c_str());
  connectStartMs = millis();
  state = WIFI_CONNECTING;
}

void wifi_init() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.length()) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    startSTA(ssid, pass);
    tryFromPrefs = true;
  } else {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    state = WIFI_FAILED;
    tryFromPrefs = false;
  }
}

void wifi_connectFromPrefs() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.length()) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    startSTA(ssid, pass);
    tryFromPrefs = true;
  } else {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    state = WIFI_FAILED;
    tryFromPrefs = false;
  }
}

void wifi_connectTemp(const String& ssid, const String& pass) {
  tempSSID = ssid;
  tempPASS = pass;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  startSTA(tempSSID, tempPASS);
  tryFromPrefs = false;
}

void wifi_stopAP() {
  WiFi.softAPdisconnect(true);
}

void wifi_startAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
}

bool wifi_isConnected() {
  return (WiFi.status() == WL_CONNECTED) && (state == WIFI_CONNECTED);
}

WifiState wifi_getState() {
  return state;
}

String wifi_getIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return String();
}

String wifi_getAPIP() {
  return WiFi.softAPIP().toString();
}

int wifi_scanNetworksBlocking() {
  return WiFi.scanNetworks();
}

//ИСПРАВИТЬ?, БЛОКИРУЕТ ВЫЗОВ если пароль от текущей сети изменился
void wifi_update() {
  if (state == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      // success
      state = WIFI_CONNECTED;

      prefs.begin("wifi", false);
      if (!tryFromPrefs && tempSSID.length()) {
        prefs.putString("ssid", tempSSID);
        prefs.putString("pass", tempPASS);
      }
      prefs.end();

      pendingApply = true;
      pendingApplyAt = millis() + APPLY_AFTER_SUCC_MS;
    } else {
      if (millis() - connectStartMs > CONNECT_TIMEOUT_MS) {
        state = WIFI_FAILED;

        tempSSID = "";
        tempPASS = "";
        tryFromPrefs = false;

        if (WiFi.softAPgetStationNum() == 0) {
          wifi_startAP();
        }
      }
    }
  }

  if (pendingApply && millis() >= pendingApplyAt) {
    wifi_stopAP();
    delay(50);
    WiFi.mode(WIFI_STA);
    pendingApply = false;
  }
}
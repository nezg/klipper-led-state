#pragma once
#include <Arduino.h>

enum WifiState {
  WIFI_IDLE = 0,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED
};

void wifi_init();

void wifi_connectTemp(const String& ssid, const String& pass);

void wifi_connectFromPrefs();

void wifi_update();

WifiState wifi_getState();

bool wifi_isConnected();

String wifi_getIP();

String wifi_getAPIP();

void wifi_startAP();

void wifi_stopAP();

int wifi_scanNetworksBlocking();
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Preferences.h>

extern Preferences prefs;
#include "printer_status.h"

void printer_moonraker_start();
void printer_moonraker_stop();
void printer_moonraker_restart();

void printer_moonraker_loop();

void printer_moonraker_getCached(PrinterStatus& out);

bool printer_moonraker_isOnline();

void printer_moonraker_requestUpdate();
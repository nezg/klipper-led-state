#include <Arduino.h>
#include <WiFi.h>
#include "wifi_manager.h"
#include "web_server.h"
#include <Preferences.h>
#include "FastLedController.h"
#include "printer_moonraker.h"

FastLedController led;
Preferences prefs;

unsigned long updateInterval = 2500;
unsigned long lastUpdate = 0;

unsigned long updateIntervalLed = 10;
unsigned long lastUpdateLed = 0;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Setup start");

  wifi_init();
  web_init();
  delay(200);

  led.begin();
  
  loadPrinterConfig(); //load IP from prefs

  //printer_moonraker_start();
  Serial.println("Setup done");
}

void loop() {
  wifi_update();
  web_handle();
  moonraker_watchdog();
  printer_moonraker_loop();

  unsigned long now = millis();
  if (now - lastUpdateLed >= updateIntervalLed) {
    PrinterStatus tmp;
    lastUpdateLed = now;
    printer_moonraker_getCached(tmp);
    led.update(tmp);
  }
}
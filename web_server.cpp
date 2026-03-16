#include "web_server.h"
#include <WebServer.h>
#include <Preferences.h>
//#include "printer_http.h"
#include "printer_status.h"   
#include "printer_moonraker.h"
#include "web_pages.h"
#include <WiFi.h>
#include "wifi_manager.h"
#include "ota_update.h"

#include <FastLED.h>
#include "FastLedController.h"

extern FastLedController led;

static WebServer server(80);
OTAUpdate ota(server);
// -----------------------------------------------
static String prefGetString(const char* ns, const char* key, const char* def) {
  prefs.begin(ns, true);
  String v = prefs.getString(key, def);
  prefs.end();
  return v;
}
// -----------------------------------------------
static uint8_t prefGetInt(const char* ns, const char* key, const uint8_t def) {
  prefs.begin(ns, true);
  uint8_t v = prefs.getUChar(key, def);
  prefs.end();
  return v;
}
// -----------------------------------------------
static uint16_t prefGetInt16(const char* ns, const char* key, const uint16_t def) {
  prefs.begin(ns, true);
  uint16_t v = prefs.getUShort(key, def);
  prefs.end();
  return v;
}
// -----------------------------------------------
const uint8_t led_pins[] = { 4, 5, 1, 2 };
const char* led_types[] = { "WS2812B", "WS2812", "WS2813", "SK6812" };
const char* colorOrders[] = { "GRB", "RGB", "BGR", "RBG" };
// ================================================================================================
static void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(MAIN_PAGE_HEAD);

  PrinterStatus s;
  printer_moonraker_getCached(s);//printer_getCached(s);
  bool online = printer_moonraker_isOnline();//printer_isOnline();

  if (!online) {
    String ip = prefGetString("printer", "ip", "");

    String pageIP = FPSTR(PRINTER_IP_PAGE);

    pageIP.replace("{{IP}}", ip);
    server.sendContent(pageIP);
  }
  server.sendContent_P(MAIN_PAGE_STATE);
  // 2) STA режим — читаем IP и цвет из prefs
  String pageMain = FPSTR(MAIN_PAGE_COLORSET);

  uint8_t brightness = prefGetInt("color", "brightness", 50);
  String standby_color = prefGetString("color", "standby_color", "#ffffff");
  String print_color = prefGetString("color", "print_color", "#529dff");
  String print_color2 = prefGetString("color", "print_color2", "#000000");
  String pause_color = prefGetString("color", "pause_color", "#ffe438");
  String error_color = prefGetString("color", "error_color", "#fe1010");
  String complete_color = prefGetString("color", "complete_color", "#24ff5b");

  uint8_t bed_temp_color_st = prefGetInt("effects", "bed_temp_c_st", 0);
  uint8_t bed_temp_color_co = prefGetInt("effects", "bed_temp_c_co", 0);
  uint8_t tempColorZonePercent = prefGetInt("effects", "t_color_zone", 10);
  uint8_t print_center_start = prefGetInt("effects", "prt_center_st", 0);
  uint8_t pause_wave_effect = prefGetInt("effects", "pause_wave_ef", 0);
  uint8_t error_wave_effect = prefGetInt("effects", "error_wave_ef", 0);
  uint8_t complete_wave_effect = prefGetInt("effects", "compl_wave_ef", 0);

  uint16_t breath_period_ms = prefGetInt16("breath", "period_ms", 2000);
  uint8_t breath_amp_percent = prefGetInt("breath", "amp_percent", 50);

  pageMain.replace("{{brightness}}", String(brightness));
  pageMain.replace("{{standby_color}}", standby_color);
  pageMain.replace("{{print_color}}", print_color);
  pageMain.replace("{{print_color2}}", print_color2);
  pageMain.replace("{{pause_color}}", pause_color);
  pageMain.replace("{{error_color}}", error_color);
  pageMain.replace("{{complete_color}}", complete_color);

  pageMain.replace("{{bed_temp_color_st_checked}}", (bed_temp_color_st > 0) ? "checked" : "");
  pageMain.replace("{{bed_temp_color_co_checked}}", (bed_temp_color_co > 0) ? "checked" : "");
  pageMain.replace("{{tempColorZonePercent}}", String(tempColorZonePercent));
  pageMain.replace("{{print_center_start_checked}}", (print_center_start > 0) ? "checked" : "");
  pageMain.replace("{{pause_wave_effect_checked}}", (pause_wave_effect > 0) ? "checked" : "");
  pageMain.replace("{{error_wave_effect_checked}}", (error_wave_effect > 0) ? "checked" : "");
  pageMain.replace("{{complete_wave_effect_checked}}", (complete_wave_effect > 0) ? "checked" : "");


  pageMain.replace("{{breath_period_ms}}", String(breath_period_ms));
  pageMain.replace("{{breath_amp_percent}}", String(breath_amp_percent));
  server.sendContent(pageMain);

  String pageLED = FPSTR(LED_CONFIG_PAGE);
  uint8_t led_pin = prefGetInt("led", "led_pin", 4);
  String led_type = prefGetString("led", "led_type", "WS2812B");
  String led_type_color = prefGetString("led", "led_type_color", "GRB");
  uint8_t led_count = prefGetInt("led", "led_count", 10);
  uint8_t led_right = prefGetInt("led", "led_right", 0);

  for (auto& p : led_pins) {
    String marker = "{{" + String(p) + "_SELECTED}}";
    pageLED.replace(marker, led_pin == p ? "selected" : "");
  }
  for (auto& c : colorOrders) {
    String marker = "{{" + String(c) + "_SELECTED}}";
    pageLED.replace(marker, led_type_color == c ? "selected" : "");
  }
  for (auto& l : led_types) {
    String marker = "{{" + String(l) + "_SELECTED}}";
    pageLED.replace(marker, led_type == l ? "selected" : "");
  }
  pageLED.replace("{{led_count}}", String(led_count));
  pageLED.replace("{{led_right_checked}}", (led_right > 0) ? "checked" : "");
  server.sendContent(pageLED);

  server.sendContent_P(WIFI_PAGE);
  server.sendContent_P(OTA_PAGE);

  server.sendContent_P(MAIN_PAGE_BOTTOM);
}

// ================================================================================================
static void handleStatus() {

  PrinterStatus s;
  printer_moonraker_getCached(s);

  bool online = printer_moonraker_isOnline();
  if (!online) {
    server.send(200, "application/json",
                "{\"state\":\"OFFLINE\",\"progress\":0,\"nozzle\":0,\"bed\":0}");
    return;
  }

  char json[256];
  // progress хранится 0..1, но при выводе JS ожидает 0..1; на клиенте умножаем на 100
  snprintf(json, sizeof(json),
           "{\"state\":\"%s\",\"progress\":%.3f,\"nozzle\":%.1f,\"bed\":%.1f}",
           s.state,
           s.progress,
           s.nozzleTemp,
           s.bedTemp);
  server.send(200, "application/json", json);
}
// ================================================================================================
static void handleSavePrint() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String ip = server.arg("ip");

  prefs.begin("printer", false);
  prefs.putString("ip", ip);
  prefs.end();

  // restart moonraker width new ip
  printer_moonraker_restart();

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
// ================================================================================================
static void handlePreview() {
  uint8_t brightness = server.arg("brightness").toInt();
  String color_value = server.arg("color");
  uint16_t breath_period_ms = server.arg("period").toInt();
  uint8_t breath_amp_percent = server.arg("amp").toInt();
  uint8_t tempColorZone = server.arg("tempzone").toInt();

  led.startPreview("#" + color_value, brightness, breath_period_ms, breath_amp_percent, tempColorZone, 2000);

  server.send(200, "text/plain", "");
}
// ================================================================================================
static void handleSaveColor() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  uint8_t brightness = server.arg("brightness").toInt();
  String standby_color = server.arg("standby_color");
  String print_color = server.arg("print_color");
  String print_color2 = server.arg("print_color2");
  String pause_color = server.arg("pause_color");
  String error_color = server.arg("error_color");
  String complete_color = server.arg("complete_color");

  uint8_t bed_temp_color_st = server.hasArg("bed_temp_color_st") ? 1 : 0;
  uint8_t bed_temp_color_co = server.hasArg("bed_temp_color_co") ? 1 : 0;
  uint8_t tempColorZonePercent = server.arg("tempColorZonePercent").toInt();
  uint8_t print_center_start = server.hasArg("print_center_start") ? 1 : 0;
  uint8_t pause_wave_effect = server.hasArg("pause_wave_effect") ? 1 : 0;
  uint8_t error_wave_effect = server.hasArg("error_wave_effect") ? 1 : 0;
  uint8_t complete_wave_effect = server.hasArg("complete_wave_effect") ? 1 : 0;

  prefs.begin("color", false);
  prefs.putUChar("brightness", brightness);
  prefs.putString("standby_color", standby_color);
  prefs.putString("print_color", print_color);
  prefs.putString("print_color2", print_color2);
  prefs.putString("pause_color", pause_color);
  prefs.putString("error_color", error_color);
  prefs.putString("complete_color", complete_color);
  prefs.end();

  prefs.begin("effects", false);
  prefs.putUChar("bed_temp_c_st", bed_temp_color_st);
  prefs.putUChar("bed_temp_c_co", bed_temp_color_co);
  prefs.putUChar("t_color_zone", tempColorZonePercent);
  prefs.putUChar("prt_center_st", print_center_start);
  prefs.putUChar("pause_wave_ef", pause_wave_effect);
  prefs.putUChar("error_wave_ef", error_wave_effect);
  prefs.putUChar("compl_wave_ef", complete_wave_effect);
  prefs.end();

  uint16_t period_ms = server.arg("breath_period_ms").toInt();
  uint8_t amp_percent = server.arg("breath_amp_percent").toInt();

  // breath namespace
  prefs.begin("breath", false);
  prefs.putUShort("period_ms", period_ms);
  prefs.putUChar("amp_percent", amp_percent);
  prefs.end();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(REBOOT_PAGE);
  delay(1000);
  ESP.restart();
}

// ================================================================================================
static void handleSaveLed() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  uint8_t led_pin = server.arg("led_pin").toInt();
  String led_type = server.arg("led_type");
  String led_type_color = server.arg("led_type_color");
  uint8_t led_count = server.arg("led_count").toInt();
  uint8_t led_right = server.hasArg("led_right") ? 1 : 0;

  prefs.begin("led", false);
  prefs.putUChar("led_pin", led_pin);
  prefs.putString("led_type", led_type);
  prefs.putString("led_type_color", led_type_color);
  prefs.putUChar("led_count", led_count);
  prefs.putUChar("led_right", led_right);
  prefs.end();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(REBOOT_PAGE);
  delay(1000);
  ESP.restart();
}
// ================================================================================================
static void handleSaveWifi() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method\"}");
    return;
  }

  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"empty_ssid\"}");
    return;
  }

  // Попытка подключения временно (не сохраняем в prefs пока не подтвердится)
  wifi_connectTemp(ssid, pass);

  server.send(200, "application/json", "{\"status\":\"connecting\"}");
}
// ================================================================================================
static void handleWifiStatus() {
  WifiState s = wifi_getState();
  String out = "{";
  if (s == WIFI_CONNECTING) {
    out += "\"status\":\"connecting\"";
  } else if (s == WIFI_CONNECTED) {
    out += "\"status\":\"connected\",";
    out += "\"ip\":\"" + wifi_getIP() + "\"";
  } else {  // WIFI_FAILED или WIFI_IDLE
    out += "\"status\":\"failed\"";
    out += ",\"ap_ip\":\"" + wifi_getAPIP() + "\"";
  }
  out += "}";
  server.send(200, "application/json", out);
}
// ================================================================================================
static void handleScan() {
  int n = wifi_scanNetworksBlocking();
  String out = "[";
  for (int i = 0; i < n; ++i) {
    if (i) out += ",";
    out += "{";
    out += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    out += "\"rssi\":" + String(WiFi.RSSI(i));
    out += "}";
  }
  out += "]";
  server.send(200, "application/json", out);
}
// ================================================================================================
static void handleEnableAP() {
  prefs.begin("wifi", false);
  prefs.putString("ssid", "");
  prefs.putString("pass", "");
  prefs.end();

  wifi_startAP();

  String out = "{";
  out += "\"status\":\"ap\",";
  out += "\"ip\":\"" + wifi_getAPIP() + "\"";
  out += "}";

  server.send(200, "application/json", out);
}
// ================================================================================================
static void handleFactoryReset() {
  server.send(200, "application/json", "{\"status\":\"resetting\"}");

  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  prefs.begin("led", false);
  prefs.clear();
  prefs.end();

  prefs.begin("color", false);
  prefs.clear();
  prefs.end();

  prefs.begin("effects", false);
  prefs.clear();
  prefs.end();

  prefs.begin("breath", false);
  prefs.clear();
  prefs.end();

  prefs.begin("printer", false);
  prefs.clear();
  prefs.end();

  delay(500);

  ESP.restart();
}
// ================================================================================================
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static void handleCPU() {
    const UBaseType_t maxTasks = 32;
    TaskStatus_t taskStatus[maxTasks];
    uint32_t totalRunTime = 0;

    // Получаем состояние всех задач и общее время
    UBaseType_t taskCount = uxTaskGetSystemState(taskStatus, maxTasks, &totalRunTime);

    // Для хранения суммарного времени IDLE по ядрам
    float idleTimes[2] = {0, 0};  // Поддержка до 2 ядер
    int coresFound = 0;

    for (UBaseType_t i = 0; i < taskCount; i++) {
        // На ESP32 задачи IDLE называются "IDLE" или "IDLE0"/"IDLE1"
        if (strstr(taskStatus[i].pcTaskName, "IDLE") != nullptr) {
            if (taskStatus[i].xCoreID < 2) {
                idleTimes[taskStatus[i].xCoreID] = (float)taskStatus[i].ulRunTimeCounter;
                if (taskStatus[i].xCoreID + 1 > coresFound) coresFound = taskStatus[i].xCoreID + 1;
            }
        }
    }

    // Вычисляем загрузку для каждого ядра
    float cpuLoad[2] = {0, 0};
    if (totalRunTime > 0) {
        for (int c = 0; c < coresFound; c++) {
            cpuLoad[c] = 100.0f * (1.0f - idleTimes[c] / (float)totalRunTime);
        }
    }

    // Формируем JSON с отдельной загрузкой по ядрам
    char json[128];
    if (coresFound == 2) {
        snprintf(json, sizeof(json), "{\"core0\":%.1f, \"core1\":%.1f}", cpuLoad[0], cpuLoad[1]);
    } else if (coresFound == 1) {
        snprintf(json, sizeof(json), "{\"core0\":%.1f}", cpuLoad[0]);
    } else {
        snprintf(json, sizeof(json), "{\"core0\":0}");
    }

    server.send(200, "application/json", json);
}
// ================================================================================================
void web_init() {
  server.on("/", handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/cpu", HTTP_GET, handleCPU);
  server.on("/saveWifi", HTTP_POST, handleSaveWifi);
  server.on("/wifi_status", HTTP_GET, handleWifiStatus);
  server.on("/enable_ap", HTTP_POST, handleEnableAP);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/preview", HTTP_GET, handlePreview);
  server.on("/savePrint", HTTP_POST, handleSavePrint);
  server.on("/saveColor", HTTP_POST, handleSaveColor);
  server.on("/saveLed", HTTP_POST, handleSaveLed);
  server.on("/reset", HTTP_POST, handleFactoryReset);

  ota.begin();

  server.begin();
}
// ================================================================================================
void web_handle() {
  server.handleClient();
}
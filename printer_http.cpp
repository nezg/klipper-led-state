//OLD, use printer_moonraker.cpp

#include "printer_http.h"
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_err.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "printer_http";

#if configNUM_CORES > 1
static const uint32_t UPDATE_PERIOD_MS = 1000;  
#else
static const uint32_t UPDATE_PERIOD_MS = 3000; 
#endif
static const uint32_t HTTP_TIMEOUT_MS = 5000;

static const uint8_t MAX_HTTP_FAILS = 3;

static const UBaseType_t PRINTER_TASK_PRIORITY = 1;
static const uint32_t PRINTER_TASK_STACK = 6 * 1024;

static char printer_ip[32] = "";
static char query_url[192];

static PrinterStatus cachedStatus = { "OFFLINE", 0.0f, 0.0f, 0.0f };
static bool cachedOnline = false;
static uint8_t httpFailCount = 0;

static SemaphoreHandle_t cachedMutex = NULL;
static TaskHandle_t printerTaskHandle = NULL;
static uint32_t taskPeriodMs = UPDATE_PERIOD_MS;

static void printerTask(void* arg);

static void setOfflineStatusLocked() {
  strncpy(cachedStatus.state, "OFFLINE", sizeof(cachedStatus.state) - 1);
  cachedStatus.state[sizeof(cachedStatus.state) - 1] = '\0';
  cachedStatus.progress = 0.0f;
  cachedStatus.nozzleTemp = 0.0f;
  cachedStatus.bedTemp = 0.0f;
  cachedOnline = false;
}

static esp_err_t _http_event_handler(esp_http_client_event_t* evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR: ESP_LOGD(TAG, "HTTP_EVENT_ERROR"); break;
    case HTTP_EVENT_ON_CONNECTED: ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED"); break;
    case HTTP_EVENT_HEADER_SENT: ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT"); break;
    case HTTP_EVENT_ON_HEADER: /* evt->data = header */ break;
    case HTTP_EVENT_ON_DATA: /* evt->data = chunk, evt->data_len */ break;
    case HTTP_EVENT_ON_FINISH: ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH"); break;
    case HTTP_EVENT_DISCONNECTED: ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED"); break;
  }
  return ESP_OK;
}

void printer_task_start(uint32_t period_ms) {
  if (!cachedMutex) cachedMutex = xSemaphoreCreateMutex();
  if (printerTaskHandle) return;

  taskPeriodMs = (period_ms == 0) ? UPDATE_PERIOD_MS : period_ms;

#if configNUM_CORES > 1
  const BaseType_t core = 0;
#else
  const BaseType_t core = tskNO_AFFINITY;
#endif

  xTaskCreatePinnedToCore(
    printerTask,
    "printer_http_task",
    PRINTER_TASK_STACK,
    nullptr,
    PRINTER_TASK_PRIORITY,
    &printerTaskHandle,
    core);
}

void printer_task_stop() {
  if (printerTaskHandle) {
    vTaskDelete(printerTaskHandle);
    printerTaskHandle = NULL;
  }
  if (cachedMutex) {
    vSemaphoreDelete(cachedMutex);
    cachedMutex = NULL;
  }
}

void printer_requestUpdate() {
  if (printerTaskHandle) {
    xTaskNotifyGive(printerTaskHandle);
  }
}

void printer_setIP(const char* ip) {
  if (!ip) return;
  strncpy(printer_ip, ip, sizeof(printer_ip) - 1);
  printer_ip[sizeof(printer_ip) - 1] = '\0';

  snprintf(query_url, sizeof(query_url),
           "http://%s:7125/printer/objects/query?print_stats=state&display_status=progress&extruder=temperature&heater_bed=temperature",
           printer_ip);

  if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    setOfflineStatusLocked();
    httpFailCount = 0;
    xSemaphoreGive(cachedMutex);
  } else {
    setOfflineStatusLocked();
    httpFailCount = 0;
  }
}

void printer_getCached(PrinterStatus& outStatus) {
  if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    outStatus = cachedStatus;
    xSemaphoreGive(cachedMutex);
  } else {
    outStatus = cachedStatus;
  }
}

bool printer_isOnline() {
  bool v = false;
  if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    v = cachedOnline;
    xSemaphoreGive(cachedMutex);
  } else {
    v = cachedOnline;
  }
  return v;
}

static void printerTask(void* arg) {
  const TickType_t waitTicks = pdMS_TO_TICKS(taskPeriodMs);

  for (;;) {
    ulTaskNotifyTake(pdTRUE, waitTicks);

    if (printer_ip[0] == '\0') {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    esp_http_client_config_t config = {};
    config.url = query_url;
    config.event_handler = _http_event_handler;
    config.timeout_ms = HTTP_TIMEOUT_MS;
    config.is_async = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
      if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
        xSemaphoreGive(cachedMutex);
      } else {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    esp_http_client_set_header(client, "Connection", "close");

    esp_err_t open_err = esp_http_client_open(client, 0);
    if (open_err != ESP_OK) {
      if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
        xSemaphoreGive(cachedMutex);
      } else {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
      }
      esp_http_client_cleanup(client);
      continue;
    }

    int fh = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);

    int total_read = 0;
    const int CHUNK = 512;
    static char buf[CHUNK];
    String body;
    if (content_length > 0) body.reserve(content_length);

    while (true) {
      int r = esp_http_client_read(client, buf, CHUNK);
      if (r > 0) {
        total_read += r;
        body.concat(buf, r);

        taskYIELD();
        continue;
      } else if (r == 0) {
        taskYIELD();
        break;
      } else {
        break;
      }
    }

    esp_http_client_close(client);

    if (total_read > 0) {
      StaticJsonDocument<1536> doc;
      DeserializationError derr = deserializeJson(doc, body);
      if (!derr) {
        if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          const char* st = doc["result"]["status"]["print_stats"]["state"] | "unknown";
          strncpy(cachedStatus.state, st, sizeof(cachedStatus.state) - 1);
          cachedStatus.state[sizeof(cachedStatus.state) - 1] = '\0';
          cachedStatus.progress = (float)(doc["result"]["status"]["display_status"]["progress"] | 0.0);
          cachedStatus.nozzleTemp = (float)(doc["result"]["status"]["extruder"]["temperature"] | 0.0);
          cachedStatus.bedTemp = (float)(doc["result"]["status"]["heater_bed"]["temperature"] | 0.0);
          cachedOnline = true;
          httpFailCount = 0;
          xSemaphoreGive(cachedMutex);
        } else {
          if (httpFailCount < 255) ++httpFailCount;
          if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
        }
      } else {
        if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          if (httpFailCount < 255) ++httpFailCount;
          if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
          xSemaphoreGive(cachedMutex);
        } else {
          if (httpFailCount < 255) ++httpFailCount;
          if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
        }
      }
    } else {
      if (cachedMutex && xSemaphoreTake(cachedMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
        xSemaphoreGive(cachedMutex);
      } else {
        if (httpFailCount < 255) ++httpFailCount;
        if (httpFailCount >= MAX_HTTP_FAILS) setOfflineStatusLocked();
      }
    }

    esp_http_client_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
#pragma once

/*
  Интерфейс получения состояния принтера Klipper через Moonraker.

  Архитектура:
  1. Основной канал — WebSocket
  2. Если WebSocket отключился → используется HTTP polling (printer_http.cpp)

  printer_moonraker.cpp обновляет локальный кэш PrinterStatus,
  который могут читать другие части прошивки (LED, WebUI и т.д.)
*/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Preferences.h>

extern Preferences prefs;
#include "printer_status.h"

/* запуск системы получения статуса */
void printer_moonraker_start();

void printer_moonraker_stop();

void printer_moonraker_restart();


/* обработка websocket (вызывать в loop()) */
void printer_moonraker_loop();

/* получить кэшированный статус */
void printer_moonraker_getCached(PrinterStatus& out);

/* проверка онлайн ли принтер */
bool printer_moonraker_isOnline();

/* форсировать обновление (например кнопкой) */
void printer_moonraker_requestUpdate();
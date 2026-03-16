//OLD, use printer_moonraker.h
#pragma once

#include <Arduino.h>
#include "printer_status.h"

void printer_task_start(uint32_t period_ms);

void printer_task_stop();

void printer_setIP(const char* ip);

void printer_requestUpdate();

void printer_getCached(PrinterStatus& outStatus);

bool printer_isOnline();
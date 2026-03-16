#pragma once

struct PrinterStatus {
    char  state[32];     // "printing", "idle", "OFFLINE", ...
    float progress;      // 0.0 .. 1.0
    float nozzleTemp;    // °C
    float bedTemp;       // °C
    //uint32_t lastUpdate;
};
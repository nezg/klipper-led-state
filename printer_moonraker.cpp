#include "printer_moonraker.h"

static WebSocketsClient ws;

static char printer_ip[16] = {0};
static PrinterStatus cachedStatus = { "OFFLINE", 0, 0, 0 };
static bool cachedOnline = false;

static uint32_t lastWsEvent = 0;
static const uint32_t WS_TIMEOUT = 3000;
static bool itTimeoutRequest = false;

static SemaphoreHandle_t statusMutex = NULL;

//===============================================================================
static void updateCachedStatus(JsonObject status)
{
    if (!statusMutex) return;

    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        if (status.containsKey("print_stats") && status["print_stats"].containsKey("state"))
            strncpy(cachedStatus.state, status["print_stats"]["state"], sizeof(cachedStatus.state)-1);

        if (status.containsKey("display_status") && status["display_status"].containsKey("progress"))
            cachedStatus.progress = status["display_status"]["progress"];

        if (status.containsKey("extruder") && status["extruder"].containsKey("temperature"))
            cachedStatus.nozzleTemp = status["extruder"]["temperature"];

        if (status.containsKey("heater_bed") && status["heater_bed"].containsKey("temperature"))
            cachedStatus.bedTemp = status["heater_bed"]["temperature"];

        cachedOnline = true;
        itTimeoutRequest = false;

        //Serial.printf("CachedStatus updated -> State: %s, Progress: %.2f, Nozzle: %.2f, Bed: %.2f\n",
        //              cachedStatus.state,
         //             cachedStatus.progress,
        //              cachedStatus.nozzleTemp,
        //              cachedStatus.bedTemp);

        xSemaphoreGive(statusMutex);
    }

    lastWsEvent = millis();
}

//===============================================================================
static void parseMoonraker(const char* payload, size_t length)
{
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload, length);

    if (err)
    {
        Serial.println("JSON parse error");
        return;
    }
    if (doc.containsKey("method") && strcmp(doc["method"], "notify_status_update") == 0) {
        JsonObject status = doc["params"][0].as<JsonObject>();
        updateCachedStatus(status);
    }
    if (doc.containsKey("result")) {
        JsonObject status = doc["result"]["status"].as<JsonObject>();
        updateCachedStatus(status);
    }

}

//===============================================================================
static void wsEvent(WStype_t type, uint8_t * payload, size_t length)
{
    switch(type)
    {
        case WStype_CONNECTED:
        {
            Serial.println("Moonraker WS connected");

            // Подписка на нужные объекты
            const char* subscribeMsg =
            "{"
            "  \"jsonrpc\": \"2.0\","
            "  \"method\": \"printer.objects.subscribe\","
            "  \"params\": {"
            "    \"objects\": {"
            "      \"print_stats\": [\"state\"],"
            "      \"display_status\": [\"progress\"],"
            "      \"extruder\": [\"temperature\"],"
            "      \"heater_bed\": [\"temperature\"]"
            "    }"
            "  },"
            "  \"id\": 2"
            "}";
            ws.sendTXT(subscribeMsg);

            // Сразу запросить начальные значения.
            printer_moonraker_requestUpdate();

            lastWsEvent = millis();
            break;
        }

        case WStype_TEXT:
            parseMoonraker((char*)payload, length);
            break;

        case WStype_DISCONNECTED:
            cachedOnline = false;
            //Serial.printf("WS disconnected, reason: %.*s\n", length, payload);
            break;

        case WStype_ERROR:
            cachedOnline = false;
            //Serial.printf("WS error: %.*s\n", length, payload);
            break;

        default:
            break;
    }
}

void printer_moonraker_start()
{
    prefs.begin("printer", true);
    String ip = prefs.getString("ip", "");
    prefs.end();

    if (ip.length() == 0) return;

    strncpy(printer_ip, ip.c_str(), sizeof(printer_ip) - 1);
    printer_ip[sizeof(printer_ip) - 1] = '\0';

    if (!statusMutex)
        statusMutex = xSemaphoreCreateMutex();

    ws.setExtraHeaders("Origin: http://localhost");
    ws.begin(printer_ip, 7125, "/websocket");
    ws.onEvent(wsEvent);
    ws.setReconnectInterval(2000);
}

//===============================================================================
void printer_moonraker_stop()
{
    if (ws.isConnected()) {
        ws.disconnect();
    }
}

//===============================================================================
void printer_moonraker_restart()
{
    printer_moonraker_stop();
    printer_moonraker_start();
}

//===============================================================================
void printer_moonraker_loop()
{
    ws.loop();

    if (millis() - lastWsEvent > WS_TIMEOUT)
    {   
        if (ws.isConnected()) {
            if (!itTimeoutRequest) {
                itTimeoutRequest = true;
                printer_moonraker_requestUpdate(); //пытаемся принудительно обновить
            } else {
                ws.disconnect(); //force disconnect.  
                cachedOnline = false;
            }
        }
        lastWsEvent = millis();
    }
}

//===============================================================================
void printer_moonraker_getCached(PrinterStatus& out)
{
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        out = cachedStatus;
        xSemaphoreGive(statusMutex);
    }
}

//===============================================================================
bool printer_moonraker_isOnline()
{
    bool v = false;
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        v = cachedOnline;
        xSemaphoreGive(statusMutex);
    }
    else
    {
        v = cachedOnline;
    }
    return v;
}

//===============================================================================
void printer_moonraker_requestUpdate()
{
    if (!ws.isConnected()) {
        return;
    }
    ws.sendTXT(
        "{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.query\","
        "\"params\":{\"objects\":{"
        "\"print_stats\": [\"state\"],"
         "\"display_status\": [\"progress\"],"
         "\"extruder\": [\"temperature\"],"
         "\"heater_bed\": [\"temperature\"]"
        "}},\"id\":1}"
    );
}
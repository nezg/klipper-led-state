#include "printer_moonraker.h"

static WebSocketsClient ws;

static PrinterStatus cachedStatus = { "OFFLINE", 0, 0, 0 };
static bool cachedOnline = false;

static uint32_t lastWsEvent = 0;
static const uint32_t WS_TIMEOUT = 3000;
static bool itTimeoutRequest = false;

static SemaphoreHandle_t statusMutex = NULL;

char printer_ip[32] = {0};
bool wsRunning = false;

uint32_t moonrakerWatchdogTimer = 0;
//===============================================================================
void updatePrinterIP(const char* newIP)
{
    prefs.begin("printer", false);
    prefs.putString("ip", newIP);
    prefs.end();

    strncpy(printer_ip, newIP, sizeof(printer_ip) - 1);
    printer_ip[sizeof(printer_ip) - 1] = '\0';

    printer_moonraker_stop();
}
//===============================================================================
void loadPrinterConfig()
{
    if (!statusMutex)
        statusMutex = xSemaphoreCreateMutex();

    prefs.begin("printer", true);

    String ip = prefs.getString("ip", "192.168.4.2");
    prefs.end();

    if (ip.length() == 0)
        return;

    strncpy(printer_ip, ip.c_str(), sizeof(printer_ip) - 1);
    printer_ip[sizeof(printer_ip) - 1] = '\0';
}
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
        //Serial.println("JSON parse error");
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
            //Serial.println("Moonraker WS connected");

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

            printer_moonraker_requestUpdate();
            //Serial.println("WS connected!!!!!!");
            lastWsEvent = millis();
            break;
        }

        case WStype_TEXT:
            parseMoonraker((char*)payload, length);
            break;

        case WStype_DISCONNECTED:
            cachedOnline = false;
            //Serial.printf("WS disconnected, reason: %.*s\n", length, payload);
            printer_moonraker_stop();
            break;

        case WStype_ERROR:
            cachedOnline = false;
            //Serial.printf("WS error: %.*s\n", length, payload);
            printer_moonraker_stop();
            break;

        default:
            break;
    }
}
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <fcntl.h>

bool checkPortAsync(const char* host, uint16_t port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;

    fcntl(sock, F_SETFL, O_NONBLOCK);

    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (res < 0 && errno != EINPROGRESS)
    {
        close(sock);
        return false;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;   // 50 ms timeout

    res = select(sock + 1, NULL, &fdset, NULL, &tv);

    if (res == 1)
    {
        int so_error = 0;
        socklen_t len = sizeof(so_error);

        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

        close(sock);

        if (so_error == 0)
            return true;  
        else
            return false;  // connection refused / timeout
    }

    close(sock);
    return false;
}
//===============================================================================
void moonraker_watchdog()
{
    if (printer_ip[0] == 0)
        return;

    if (millis() - moonrakerWatchdogTimer < 2000)
        return;

    if (ws.isConnected())
        return;

    //Serial.print("---------------Watchdog   ");    
    moonrakerWatchdogTimer = millis();
    
    bool available = checkPortAsync(printer_ip, 7125);
    //Serial.println(available);
    if (available && !wsRunning)
    {   
        wsRunning = true;
        //Serial.println("Watchdog start");
        printer_moonraker_start();
    }
}
//===============================================================================
void printer_moonraker_start()
{
    ws.setExtraHeaders("Origin: http://localhost");
    ws.begin(printer_ip, 7125, "/websocket");
    ws.onEvent(wsEvent);
    ws.setReconnectInterval(1000);
}

//===============================================================================
void printer_moonraker_stop()
{
    if (ws.isConnected()) {
        ws.disconnect();
        if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            cachedStatus = { "OFFLINE", 0, 0, 0 };
            xSemaphoreGive(statusMutex);
        }
    }
    wsRunning = false;
    //Serial.println("WS STOPPED");
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
    if (!wsRunning) 
        return;

    ws.loop();
    if (millis() - lastWsEvent > WS_TIMEOUT)
    {   
        if (ws.isConnected()) {
            if (!itTimeoutRequest) {
                itTimeoutRequest = true;
                //Serial.println("Force request");
                printer_moonraker_requestUpdate(); 

            } else {
                //Serial.println("Force stop");
                printer_moonraker_stop();
                cachedOnline = false;
            }
        } else {
            wsRunning = false; 
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
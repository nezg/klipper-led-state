#include "ota_update.h"
#include <Update.h>

OTAUpdate::OTAUpdate(WebServer& srv) : server(srv) {}

void OTAUpdate::begin()
{
    server.on("/update", HTTP_POST,
        [this]() { handleFinish(); },
        [this]() { handleUpload(); }
    );
}

void OTAUpdate::handleUpload()
{
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("OTA Start: %s\n", upload.filename.c_str());

        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            Update.printError(Serial);
        }
    }

    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            Update.printError(Serial);
        }
    }

    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
        {
            Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
        }
        else
        {
            Update.printError(Serial);
        }
    }
}

void OTAUpdate::handleFinish()
{
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK, REBOOT");

    delay(1000);
    ESP.restart();
}
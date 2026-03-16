#pragma once
#include <WebServer.h>

class OTAUpdate {
public:
    OTAUpdate(WebServer& server);
    void begin();

private:
    WebServer& server;

    void handleUpload();
    void handleFinish();
};
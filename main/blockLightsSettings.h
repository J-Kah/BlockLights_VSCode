#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>

#include "sharedData.h"

extern WebSocketsServer webSocket;  // Access the WebSocket server instance
extern settings_t settings;

// Declare the initialization function for routes
void initSettingsRoutes(WebServer &server);



#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

#include "sharedData.h"

extern bool blinking;
extern WebSocketsServer webSocket;  // Access the WebSocket server instance

// Declare the initialization function for routes
void initSetupRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSetupUpdates();

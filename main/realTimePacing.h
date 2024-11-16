#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

#include "sharedData.h"

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

extern void startRealTimePacingTask();

// Declare the initialization function for routes
void initRealTimePacingRoutes(WebServer &server);

void realTimePacingTask(void* pvParameters) ;

// Function to update realTimePacing values in real time
void sendRealTimePacingUpdates();

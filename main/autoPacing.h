#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h> // arduino

#include "sharedData.h"

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

extern void startAutoPacingTask();

// Declare the initialization function for routes
void initAutoPacingRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendAutoPacingUpdates();

void autoPacingTask(void* pvParameters) ;
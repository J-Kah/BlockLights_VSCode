#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

#include "sharedData.h"

// Declare the real-time pacing fucnction
extern void startRealTimePacingTask();

// Declare the initialization function for routes
void initRealTimePacingRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendRealTimePacingUpdates();

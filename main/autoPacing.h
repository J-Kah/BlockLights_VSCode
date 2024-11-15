#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>

#include "sharedData.h"

// Declare the real-time pacing fucnction
extern void startAutoPacingTask();

// Declare the initialization function for routes
void initAutoPacingRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendAutoPacingUpdates();
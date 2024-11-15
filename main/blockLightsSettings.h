#pragma once

#include <WebServer.h>
#include <WebSocketsServer.h>

#include "sharedData.h"

// Declare the struct and global variables as before
typedef struct {
    bool numberOfTracks;
    int trackNumber;
    bool startingOn500mSide;
    int numLeadingBlocks;
    int numPacingBlocks;
    int numTrailingBlocks;
    bool showAllBlocks;
} settings_t;

extern settings_t settings;

// Declare the initialization function for routes
void initSettingsRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSettingsUpdates();

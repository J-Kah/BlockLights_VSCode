#ifndef SETTINGS_H
#define SETTINGS_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS
#include <sharedData.h>

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
extern bool reDir;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

extern void killPacingTasks();

extern void scanForBlocks();

extern void addAllBlocks();

extern void removePeer(uint8_t* peerMAC);

// Declare redirect function
extern int ensureRedirect(String path);

// Declare the initialization function for routes
void initSettingsRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSettingsUpdates();

#endif
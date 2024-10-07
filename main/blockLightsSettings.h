#ifndef SETTINGS_H
#define SETTINGS_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS

// Declare the struct and global variables as before
typedef struct {
    bool numberOfTracks;
    int trackNumber;
    bool startingOn500mSide;
    int numLeadingBlocks;
    int numPacingBlocks;
    int numTrailingBlocks;
} settings_t;

extern settings_t settings;
extern bool reDir;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

// Declare the initialization function for routes
void initSettingsRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSettingsUpdates();

#endif
#ifndef BLOCKLIGHTSSETUP_H
#define BLOCKLIGHTSSETUP_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS
#include <sharedData.h>


extern bool reDir;
extern bool blinking;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

extern void blinkBlock(String mac, int idx);

extern void blinkAll();

extern void killPacingTasks();

// Declare the initialization function for routes
void initSetupRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSetupUpdates();

#endif
#ifndef AUTOPACING_H
#define AUTOPACING_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS

// Declare the struct and global variables as before
typedef struct {
    String autoStatus;
    float autoLaps;
    float autoTime;
    bool autoCountdown;
    bool autoIsRunning;
} autoPacing_t;

extern autoPacing_t autoPacing;
extern bool reDir;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

// Declare the real-time pacing fucnction
extern void startAutoPacingTask();

// Declare the initialization function for routes
void initAutoPacingRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendAutoPacingUpdates();

#endif
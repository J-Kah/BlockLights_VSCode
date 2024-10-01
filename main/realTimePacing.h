#ifndef REALTIMEPACING_H
#define REALTIMEPACING_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS

// Declare the struct and global variables as before
typedef struct {
    String status;
    int lap;
    float lapTime;
    bool is_running;
} realTimePacing_t;

extern realTimePacing_t realTimePacing;
extern bool reDir;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

// Declare the initialization function for routes
void initRealTimePacingRoutes(WebServer &server);

// Function to reset the realTimePacing struct to initial values
void resetRealTimePacing();

// Functino to update realTimePacing values in real time
void sendRealTimePacingUpdates();

#endif
#ifndef REALTIMEPACING_H
#define REALTIMEPACING_H

#include <WebServer.h>
#include <SPIFFS.h>  // Assuming you are using SPIFFS
#include <sharedData.h>

// Declare the struct and global variables as before
typedef struct {
    String status;
    int lap;
    float lapTime;
    bool is_running;
} realTimePacing_t;

extern realTimePacing_t realTimePacing;
extern bool reDir;
extern std::vector<block_t> blocks;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

// Declare the real-time pacing fucnction
extern void startRealTimePacingTask();

extern void killPacingTasks();

// Declare the initialization function for routes
void initRealTimePacingRoutes(WebServer &server);

// Function to reset the realTimePacing struct to initial values
void resetRealTimePacing();

// Function to update realTimePacing values in real time
void sendRealTimePacingUpdates();

#endif
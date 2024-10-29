#ifndef BLOCKLIGHTSSETUP_H
#define BLOCKLIGHTSSETUP_H

#include <WebServer.h>
#include "sharedData.h"


extern bool reDir;
extern bool blinking;

// Declare serveFile function
extern void serveFile(const char *filename, const char *contentType);

// Declare redirect function
extern int ensureRedirect(String path);

extern void blinkBlock(String mac, int idx);

extern void blinkAll();

extern void killPacingTasks();

extern void scanForBlocks();

extern void updateBlockNumber(int idx, int num);

extern void removePeer(uint8_t* peerMAC);

extern void writeBlocksToFile();

// Declare the initialization function for routes
void initSetupRoutes(WebServer &server);

// Function to update realTimePacing values in real time
void sendSetupUpdates();

#endif
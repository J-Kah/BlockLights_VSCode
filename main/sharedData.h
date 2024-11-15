#pragma once

#include <FastLED.h>
#include <string>
#include <WebServer.h> // arduino
#include <esp_now.h>

#define NUM_LEDS 3          // Number of LEDs in your strip
#define DATA_PIN 8          // Pin connected to your LED strip
#define CHANNEL 1

// struct for each of the blocks
struct block_t {
    uint8_t mac[6];      // MAC address of the slave
    int rssi;            // signal strength
    std::string status;       // online/offline or active status
    int number;          // number of block along track
    std::string blockId;      // string dentifier for the block
    CRGB colour;         // color associated with the block
};

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Master's MAC address (for scan)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;

class pacingSuper {
    public:
    std::string status;
    float laps;
    bool isRunning;
    
    // constructor
    pacingSuper(std::string status, float laps, bool isRunning) :  status(status), laps(laps), isRunning(isRunning) {}

    // destructor
    virtual ~pacingSuper(){}

};

// realTimePacing class
class realTimePacingClass : public pacingSuper {
    public:
    float lapTime;

    // constructor
    realTimePacingClass(std::string status, float laps, bool isRunning, float lapTime) : pacingSuper(status, laps, isRunning), lapTime(lapTime) {}

};

// autoPacing class
class autoPacingClass : public pacingSuper {
    public:
    float totalTime; 
    bool countdown;

    // constructor
    autoPacingClass(std::string status, float totalTime, float laps, bool isRunning, bool countdown) : pacingSuper(status, laps, isRunning), totalTime(totalTime), countdown(countdown) {}

};

// function declarations
void writeBlocksToFile();
void readBlocksFromFile();
std::string macToString(const uint8_t* mac);
int ensureRedirect(std::string path);
void serveFile(const char *filename, const char *contentType);
void killPacingTasks(realTimePacingClass& realTimePacing, autoPacingClass& autoPacing);
void updateLEDs();
void scanForBlocks();
void addAllBlocks();
void blinkAll();
void blinkBlock(std::string mac, int idx);
void addPeer(uint8_t* peerMAC);
void removePeer(uint8_t* peerMAC);
void updateBlockNumber(int idx, int num);
void updateLED(int i);

// variable declarations that are defined in sharedData.cpp but need to be used in other files
extern bool reDir;
extern bool blinking;
extern int blinkBlockIdx;
extern realTimePacingClass realTimePacing;
extern autoPacingClass autoPacing;
extern std::vector<block_t> blocks;
extern CRGB leds[];
extern WebServer server;

// variables that are used in sharedData.cpp but are defined in other files
extern uint8_t masterMacAddress[6];
extern uint8_t broadcastMAC[];

// functions that are used in sharedData.cpp but are defined in other files
extern void sendRealTimePacingUpdates();
extern void sendAutoPacingUpdates();
extern void sendSetupUpdates();

#pragma once

#include <FastLED.h>
#include <iostream>
#include <WebServer.h> // arduino
#include <esp_now.h>

#define NUM_LEDS 3          // Number of LEDs in your strip
#define DATA_PIN 8          // Pin connected to your LED strip
#define CHANNEL 1

// struct for each of the blocks
struct block_t {
    uint8_t mac[6];             // MAC address of the slave
    std::string status;         // Connected / disconnected status
    int number;                 // Number of block along track
    std::string blockId;        // String identifier for the block
    CRGB::HTMLColorCode colour; // Colour associated with the block
};

// Declare the struct and global variables as before
struct settings_t{
    bool numberOfTracks;
    int trackNumber;
    bool startingOn500mSide;
    int numLeadingBlocks;
    int numPacingBlocks;
    int numTrailingBlocks;
    bool showAllBlocks;
} ;

enum messageType_t {
    SCAN = 0,      // 0 for scan (expecting a response)
    LED_COLOR_CHANGE,   // 1 for slave LED color change
    SLAVE_BLOCK_UPDATE,  // 2 for updating slave block number
    SLAVE_REPLY         // 3 for slave replying to master
};

struct message_t {
    messageType_t type;      // Message type
    uint8_t mac[6];    // Master's MAC address (for scan reply)
    CRGB::HTMLColorCode colour;    // RGB values (for LED color change)
    int number;    // Slave block number (to be updated to, or replied with)
};

// variable declarations that are defined in sharedData.cpp but need to be used in other files
extern bool reDir;
extern bool blinking;
extern int blinkBlockIdx;
extern std::vector<block_t> blocks;
extern CRGB leds[];
extern WebServer server;
extern const float trackLength;
extern const float blockDistances[14];

// functions that are used in sharedData.cpp but are defined in other files
extern void sendRealTimePacingUpdates();
extern void sendAutoPacingUpdates();
extern void sendSetupUpdates();
extern void realTimePacingTask(void* pvParameters);
extern void autoPacingTask(void* pvParameters);

// variables/objects that are used in sharedData.cpp but are defined in other files
extern uint8_t masterMacAddress[6];
extern const uint8_t broadcastMAC[];
extern settings_t settings;

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

    void startRealTimePacingTask(pacingSuper& autoPacing) {
        autoPacing.isRunning = false;
        xTaskCreate(&realTimePacingTask, "realTimePacingTask", 4096, NULL, 5, NULL);
    };
};

// autoPacing class
class autoPacingClass : public pacingSuper {
    public:
    float totalTime; 
    bool countdown;

    // constructor
    autoPacingClass(std::string status, float totalTime, float laps, bool isRunning, bool countdown) : pacingSuper(status, laps, isRunning), totalTime(totalTime), countdown(countdown) {}

    void startAutoPacingTask(pacingSuper& realTimePacing) {
        realTimePacing.isRunning = false;
        if(int(2*this->laps) % 2 == 0) {
            settings.startingOn500mSide = false;
        } else {
            settings.startingOn500mSide = true;
        }
        xTaskCreate(&autoPacingTask, "autoPacingTask", 4096, NULL, 5, NULL);
    }
};

extern realTimePacingClass realTimePacing;
extern autoPacingClass autoPacing;

// function declarations that need to be used in other files
void writeBlocksToFile();
void readBlocksFromFile();
std::string macToString(const uint8_t* mac);
int ensureRedirect(const std::string& path);
void serveFile(const char *filename, const char *contentType);
void killPacingTasks(realTimePacingClass& realTimePacing, autoPacingClass& autoPacing);
void updateLEDs();
void scanForBlocks();
void blinkAll();
void blinkBlock(const std::string& mac, int idx);
void addPeer(const uint8_t* peerMAC);
void removePeer(const uint8_t* peerMAC);
void updateBlockNumber(int idx, int num);
void countdown();
void updateBlock(int idx, const CRGB::HTMLColorCode& colour, bool* blocksChanged);
String CRGBtoHexString(const CRGB::HTMLColorCode& colour);
void getOffsets(float &blueOffset, float &greenOffset, float &redOffset, float &offOffset, float &trackOffset);
void updateAllBlocks(const CRGB::HTMLColorCode& colour);



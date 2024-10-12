#ifndef SHAREDDATA_H
#define SHAREDDATA_H

#include <stdint.h>  // for uint8_t
#include <FastLED.h>

// Struct for each of the blocks
struct block_t {
    uint8_t mac[6];      // MAC address of the slave
    int rssi;            // Signal strength
    String status;       // Online/offline or active status
    int number;          // number of block along track
    String blockId;      // Identifier for the block
    CRGB colour;         // Color associated with the block
};

extern std::vector<block_t> blocks;

// Declare the array of structs, but don't initialize it yet
//extern block_t blocks[];

#endif // SHAREDDATA_H
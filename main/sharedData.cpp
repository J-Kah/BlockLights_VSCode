#include "sharedData.h"

bool reDir = 1;
bool blinking = 0;
int blinkBlockIdx;
std::string blinkBlockMac;
WebServer server(80);
CRGB leds[NUM_LEDS];

static const float interBlockDistance = 4.4506;
// track length (Based off of metric distances, 8.5m radius with 28.85m straights)
const float trackLength = 111.1071;

// Array of floats that holds the distances of the blocks around the track (May need to be tweaked if timing motion is unrealistic around the corner).
// Based off of ISU Short Track Special Regulations and Technical Rules.
// Ttrack offsets for standing starts, .5 lap starts are 1m apart for 5 tracks or 0.7m for 7 tacks. The furtherest forward track on 500m start line is track 1.   
const float blockDistances[14] = {
    14.4250,  // First block (master block)
    18.8756,
    23.3262,
    27.7768, // Apex block of first corner
    32.2274,
    36.6779,
    41.1285, // last block of first corner

    69.9785,// first block of second corner
    74.4291,
    78.8797,
    83.3303, // apex block of second corner
    87.7809,
    92.2315,
    96.6821 // last block of second corner
};

// Array of structs to hold all the blocks
std::vector<block_t> blocks;

// Function to write array of structs to file in the custom format
void writeBlocksToFile() {

    // Open the file for writing using standard fopen (overwrite mode)
    FILE* file = fopen("/partition/blocks.txt", "w");  // "w" mode will overwrite the file
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Iterate through the blocks and write data to the file
    for (int i = 1; i < blocks.size(); i++) {
        // Write MAC address as a string followed by a separator
        fputs(macToString(blocks[i].mac).c_str(), file);
        fputc(';', file);  // Separator between MAC address and number

        // Write the integer value
        fprintf(file, "%d", blocks[i].number);

        // Add comma between devices, except for the last one
        if (i < blocks.size() - 1) {
            fputc(',', file);
        }
    }

    // Close the file after writing
    fclose(file);
    Serial.println("Data written successfully.");
}

// Function to convert MAC address byte array to string
std::string macToString(const uint8_t* mac) {
    char buffer[18];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buffer);
}

int ensureRedirect(const std::string& path) {
    // Redirect to blocklights.local
    if(reDir) {
        reDir = 0;
        std::string URL= "http://blocklights.local" + path;
        server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        server.sendHeader("Location", URL.c_str(), true); // 301 (temporary) or 302 (permanent) redirect
        server.send(301);
        return 1;
    }
    else {
        reDir = 1;
        return 0;
    }
}

// Function to serve files from SPIFFS
void serveFile(const char *filename, const char *contentType) {
    // Create the full path using String
    std::string fullPath = std::string("/partition/") + filename;

    // Open the file for reading
    FILE* file = fopen(fullPath.c_str(), "r");
    if (file) {
        // Get file size
        struct stat fileInfo;
        fstat(fileno(file), &fileInfo);
        size_t fileSize = fileInfo.st_size;

        // Set content length
        server.setContentLength(fileSize);  // Set known length

        // Send the header
        server.send(200, contentType, "");  // Start the response

        // Stream file content to client
        char buffer[128];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            server.client().write(buffer, bytesRead);  // Send chunks to the client
        }

        fclose(file);  // Close the file when done
    } else {
        server.send(404, "text/plain", "File Not Found");
    }
}

void killPacingTasks(realTimePacingClass& realTimePacing, autoPacingClass& autoPacing){
    realTimePacing.isRunning = false;
    autoPacing.isRunning = false;

    // turn blocks off
    vTaskDelay(pdMS_TO_TICKS(1000));
    updateAllBlocks(CRGB::Black);
}

void updateLEDs(){
    fill_solid(leds, NUM_LEDS, blocks[0].colour);
    FastLED.show();
    if(realTimePacing.isRunning){
        sendRealTimePacingUpdates();
    } else if(autoPacing.isRunning){
        sendAutoPacingUpdates();
    }

    // SEND ESP NOW COMMAND to all slaves
    for(int i=1; i< blocks.size(); i++) {
        if(blocks[i].status != "Virtual") {
            message_t msg;
            msg.type = LED_COLOR_CHANGE;  // Type 1: LED color change
            msg.colour = blocks[i].colour;

            esp_err_t result = esp_now_send(blocks[i].mac, (uint8_t *)&msg, sizeof(msg));

            if (result == ESP_OK) {
                Serial.println("LED color change message sent successfully");
            } else {
                Serial.println("Error sending LED color change message");
            }
        }
    }
}

void scanForBlocks() {
    Serial.println("Scanning for ESP-NOW slaves...");

    message_t msg;
    msg.type = SCAN;  // Type 0: Scan
    memcpy(msg.mac, masterMacAddress, 6);  // Include Master's MAC address in the scan

    // Send broadcast message to all devices
    esp_err_t result = esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));
    
    if (result == ESP_OK) {
        Serial.println("Broadcast scan message sent successfully");
    } else {
        Serial.println("Error sending scan message");
    }
}

void removePeer(const uint8_t* peerMAC) {
    if (esp_now_del_peer(peerMAC) == ESP_OK) {
        Serial.println("Peer removed successfully");
    } else {
        Serial.println("Failed to remove peer");
    }
}

static void updateLED(int i){
    if(i == 0) {
        fill_solid(leds, NUM_LEDS, blocks[0].colour);
        FastLED.show();
    } else if(blocks[i].status != "Virtual") {
        blinkBlockIdx = i;

        message_t msg;
        msg.type = LED_COLOR_CHANGE;  // Type 1: LED color change
        msg.colour = blocks[i].colour;

        esp_err_t result = esp_now_send(blocks[i].mac, (uint8_t *)&msg, sizeof(msg));

        if (result == ESP_OK) {
            Serial.println("LED color change message sent successfully");
        } else {
            Serial.println("Error sending LED color change message");
        }
    }
}

static void blinkAllTask(void* Parameters){    
    for(int i=0; i < blocks.size(); i++){
        if(blocks[i].status != "Virtual") {
            blocks[i].status = "Blinking...";
            sendSetupUpdates();

            blocks[i].colour = CRGB::Blue;
            updateLED(i);
            vTaskDelay(500 / portTICK_PERIOD_MS);

            blocks[i].colour = CRGB::Black;
            updateLED(i);
            vTaskDelay(500 / portTICK_PERIOD_MS);

            if(i == 0){
                blocks[i].status = "Master";
            } else if(blocks[i].status != "Disconnected") {
                blocks[i].status = "Working";
            }
            sendSetupUpdates();
        }
        
    }
    blinking = 0;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);  // Delete this task

}

void blinkAll(){
    xTaskCreate(&blinkAllTask, "blinkAllTask", 4096, NULL, 5, NULL);
}

static void blinkBlockTask(void* Parameters){
    //update block status to blinking    
    blocks[blinkBlockIdx].status = "Blinking...";
    sendSetupUpdates();
    
    for(int i = 0; i < 3; i++) {
        blocks[blinkBlockIdx].colour = CRGB::Blue;
        updateLED(blinkBlockIdx);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        blocks[blinkBlockIdx].colour = CRGB::Black;
        updateLED(blinkBlockIdx);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    if(blinkBlockIdx == 0) {
        blocks[blinkBlockIdx].status = "Master"; 
    } else if(blocks[blinkBlockIdx].status != "Disconnected") {
        blocks[blinkBlockIdx].status = "Working";
    }
    sendSetupUpdates();

    blinking = 0;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);  // Delete this task

}

void blinkBlock(const std::string& mac, int idx) {
    blinkBlockMac = mac;
    blinkBlockIdx = idx;
    xTaskCreate(&blinkBlockTask, "blinkBlockTask", 4096, NULL, 5, NULL);
}

void addPeer(const uint8_t* peerMAC) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, peerMAC, 6);  // Use the specific MAC address


    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("Peer added successfully");
    } else {
        Serial.println("Failed to add peer");
    }
}

void updateBlockNumber(int idx, int num) {
    message_t msg;
    msg.type = SLAVE_BLOCK_UPDATE;
    msg.number = num;

    esp_err_t result = esp_now_send(blocks[idx].mac, (uint8_t *)&msg, sizeof(msg));

    if (result == ESP_OK) {
        Serial.println("Block number update message sent successfully");
    } else { 
        blocks[idx].status = "Disconnected"; 
        sendSetupUpdates();
        Serial.println("Error sending block number update message");
    }
}

// I think this won't work becuase the send should always send, but can't check if it was received.
// Function to read from file and populate the array of structs
void readBlocksFromFile() {

    int maxBlocks = 14;

    // Open the file for reading using fopen in "r" (read) mode
    FILE* file = fopen("/partition/blocks.txt", "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    // Read the entire file into a string
    std::string content;
    char buffer[281];
    while (fgets(buffer, sizeof(buffer), file) != nullptr) {
        content += std::string(buffer);  // Append each line to the content
    }
    fclose(file);  // Close the file

    // Split the content by commas to get individual device entries
    int blockCount = 0;
    int start = 0;
    while (start < content.length() && blockCount < maxBlocks) {
        block_t newBlock;

        // Find the position of the next comma starting from 'start'
        size_t end = content.find(',', start);
        if (end == std::string::npos) {
            end = content.length();
        }

        // Extract the substring between 'start' and 'end'
        std::string entry = content.substr(start, end - start);

        // Find the separator index (';')
        size_t separatorIndex = entry.find(';');

        // Get the MAC address and integer value as substrings
        std::string macStr = entry.substr(0, separatorIndex);
        std::string intStr = entry.substr(separatorIndex + 1);

        // Parse MAC address and integer
        // parseMACAddress(macStr, newBlock.mac);
        sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &newBlock.mac[0], &newBlock.mac[1], &newBlock.mac[2], &newBlock.mac[3], &newBlock.mac[4], &newBlock.mac[5]);
        
        // check if it's a virtual block:
        if(newBlock.mac[0] != 0x00 && newBlock.mac[1] != 0x00 && newBlock.mac[2] != 0x00 && newBlock.mac[3] != 0x00 && newBlock.mac[4] != 0x00) {
            newBlock.number = std::stoi(intStr);
            newBlock.colour = CRGB::Black;
            newBlock.blockId = "block" + std::to_string(newBlock.number);
            newBlock.status = "Disconnected";

            blocks.push_back(newBlock);
            Serial.println("Pushed a new block");

            blockCount++;
        }
        start = end + 1;
    }

    Serial.println("Data read successfully.");
    return;
}

void updateAllBlocks(const CRGB::HTMLColorCode& colour) {
    for (int i = 0; i < size(blocks); i++) {
        blocks[i].colour = colour;
    }
    updateLEDs();
}

void countdown() {
    Serial.println("Starting countdown...");

    uint64_t countDownStartTime = esp_timer_get_time();
    uint64_t currentTime = countDownStartTime;
    while(autoPacing.isRunning && currentTime - countDownStartTime < 3000000){
        currentTime = esp_timer_get_time();
        if (currentTime - countDownStartTime > 0 && currentTime - countDownStartTime < 500000 && blocks[0].colour != CRGB::Red) {
            updateAllBlocks(CRGB::Red);
        } else if (currentTime - countDownStartTime > 500000 && currentTime - countDownStartTime < 1000000 && blocks[0].colour != CRGB::Black) {
            updateAllBlocks(CRGB::Black);
        } else if (currentTime - countDownStartTime > 1000000 && currentTime - countDownStartTime < 1500000 && blocks[0].colour != CRGB::Red) {
            updateAllBlocks(CRGB::Red);
        } else if (currentTime - countDownStartTime > 1500000 && currentTime - countDownStartTime < 2000000 && blocks[0].colour != CRGB::Black) {
            updateAllBlocks(CRGB::Black);
        } else if (currentTime - countDownStartTime > 2000000 && currentTime - countDownStartTime < 2500000 && blocks[0].colour != CRGB::Orange) {
            updateAllBlocks(CRGB::Orange);
        } else if (currentTime - countDownStartTime > 2500000 && blocks[0].colour != CRGB::Black) {
            updateAllBlocks(CRGB::Black);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
    }
    updateAllBlocks(CRGB::Green);
    Serial.println("Finishing countdown");
}

void updateBlock(int idx, const CRGB::HTMLColorCode& colour, bool* blocksChanged){

    if(blocks[idx].colour != colour) {
        // change block to off
        blocks[idx].colour = colour;
        updateLED(idx);
        *blocksChanged = true;

        std::string colourString;
        switch(colour) {
            case CRGB::Black:
                colourString = "off";
                break;
            case CRGB::Red:
                colourString = "red";
                break;
            case CRGB::Green:
                colourString = "green";
                break;
            case CRGB::Blue:
                colourString = "blue";
                break;
            default:
                break;
        }
        std::cout << "Changed block " << (idx + 1) << " to colour " << colourString << std::endl;
    }
}

String CRGBtoHexString(const CRGB::HTMLColorCode& colour) {

    switch (colour) {
        case CRGB::Red:
            return "#FF0000";
        case CRGB::Green:
            return "#00FF00";
        case CRGB::Blue:
            return "#0000FF";
        case CRGB::Orange:
            return "#FFA500";
        default:
            return "#000000";
    }
}

void getOffsets(float &blueOffset, float &greenOffset, float &redOffset, float &offOffset, float &trackOffset){
    // figure out distance offset from pacing distance to each light
    // turn blue distance in front
    blueOffset = ((settings.numPacingBlocks-1)/2.0  + settings.numLeadingBlocks) * interBlockDistance;
    // turn green distance in front
    greenOffset = ((settings.numPacingBlocks-1)/2.0) * interBlockDistance;
    // turn red distance behind
    redOffset = -(((settings.numPacingBlocks-1)/2.0 + 1) * interBlockDistance);
    // turn off distance behind
    offOffset = -(((settings.numPacingBlocks-1)/2.0 + 1 + settings.numTrailingBlocks) * interBlockDistance);

    // Vary the offsets based on which track we are on:
    if(settings.numberOfTracks) {
        trackOffset = (settings.trackNumber - 4)*0.7*(1.0 + settings.startingOn500mSide);
    } else {
        trackOffset = (settings.trackNumber - 3)*(1.0 + settings.startingOn500mSide);
    } 
}




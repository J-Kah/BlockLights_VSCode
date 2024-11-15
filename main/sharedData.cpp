#include "sharedData.h"

bool reDir = 1;
bool blinking = 0;
int blinkBlockIdx;
std::string blinkBlockMac;
WebServer server(80);
CRGB leds[NUM_LEDS];



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

int ensureRedirect(std::string path) {
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
    for(int i = 0; i < blocks.size(); i++){
        blocks[i].colour = CRGB::Black;
    }
    updateLEDs();
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
            Message msg;
            msg.type = 1;  // Type 1: LED color change
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

    Message msg;
    msg.type = 0;  // Type 0: Scan
    memcpy(msg.mac, masterMacAddress, 6);  // Include Master's MAC address in the scan

    // Send broadcast message to all devices
    esp_err_t result = esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));
    
    if (result == ESP_OK) {
        Serial.println("Broadcast scan message sent successfully");
    } else {
        Serial.println("Error sending scan message");
    }
}

void addAllBlocks(){
    for(int i=2; i<=14; i++){
        bool inBlocks = false;
        for(int j = 0; j < blocks.size(); j++) {
            if(blocks[j].number == i) {
                inBlocks = true;
            }
        }  
        if(!inBlocks) {
            block_t block = { {0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)i}, 0, "Virtual", i, "block" + std::to_string(i), CRGB::Black};
            blocks.push_back(block);
        } 
    }
    sendSetupUpdates();
}

void removePeer(uint8_t* peerMAC) {
    if (esp_now_del_peer(peerMAC) == ESP_OK) {
        Serial.println("Peer removed successfully");
    } else {
        Serial.println("Failed to remove peer");
    }
}

void blinkAllTask(void* Parameters){    
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

void updateLED(int i){
    if(i == 0) {
        fill_solid(leds, NUM_LEDS, blocks[0].colour);
        FastLED.show();
    } else if(blocks[i].status != "Virtual") {
        blinkBlockIdx = i;

        Message msg;
        msg.type = 1;  // Type 1: LED color change
        msg.colour = blocks[i].colour;

        esp_err_t result = esp_now_send(blocks[i].mac, (uint8_t *)&msg, sizeof(msg));

        if (result == ESP_OK) {
            Serial.println("LED color change message sent successfully");
        } else {
            Serial.println("Error sending LED color change message");
        }
    }
}

void blinkBlockTask(void* Parameters){
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

void blinkBlock(std::string mac, int idx) {
    blinkBlockMac = mac;
    blinkBlockIdx = idx;
    xTaskCreate(&blinkBlockTask, "blinkBlockTask", 4096, NULL, 5, NULL);
}

void addPeer(uint8_t* peerMAC) {
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
    Message msg;
    msg.type = 2;
    msg.number = num;
    
    uint8_t slaveMAC[6];
    for(int j = 0; j < 6; j++) {
        slaveMAC[j] = blocks[idx].mac[j];
    }

    esp_err_t result = esp_now_send(slaveMAC, (uint8_t *)&msg, sizeof(msg));

    if (result == ESP_OK) {
        Serial.println("Block number update message sent successfully");
    } else {
        blocks[idx].status = "Disconnected";
        sendSetupUpdates();
        Serial.println("Error sending block number update message");
    }
}

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
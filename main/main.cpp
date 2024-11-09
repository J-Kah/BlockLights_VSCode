/*
 * BlockLights
 * Author: Joshua Kah
 * Contact: joshk995@gmail.com
 * Year: 2024
 *
 * This code is written by Joshua Kah and is provided under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 * 
 * You are free to:
 *  - Share: Copy and redistribute the material in any medium or format
 *  - Adapt: Remix, transform, and build upon the material, provided it is for non-commercial purposes.
 * 
 * Under the following terms:
 *  - Attribution: You must give appropriate credit, provide a link to the license, and indicate if changes were made.
 *  - NonCommercial: You may not use the material for commercial purposes.
 *  - ShareAlike: If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
 * 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
 * 
 * This code is provided "as-is" without any warranties or guarantees.
 */



#include <WebServer.h> // arduino
#include <ESPmDNS.h> // arduino
#include <DNSServer.h> // arduino
#include <WebSocketsServer.h> // arduino

// Hopefully the above can be replaced with ESPAsyncWebServer or similar.

#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_littlefs.h>
#include <FastLED.h>
#include <esp_now.h>

//Custom libraries
#include "realTimePacing.h"
#include "blockLightsSettings.h"
#include "autoPacing.h"
#include "blockLightsSetup.h"
#include "sharedData.h"


#define NUM_LEDS 1          // Number of LEDs in your strip
#define DATA_PIN 8          // Pin connected to your LED strip
#define CHANNEL 11

CRGB leds[NUM_LEDS];

// DNS server
DNSServer dnsServer;

WebServer server(80);                 // Create a web server on port 80
WebSocketsServer webSocket = WebSocketsServer(81);  // Create WebSocket on port 81

bool reDir = 1;
bool blinking = 0;

IPAddress apIP(192, 168, 4, 1);
uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast address for scan
uint8_t masterMacAddress[6];
String masterMacAddressString;

String blinkBlockMac;
int blinkBlockIdx;

// Array of structs to hold all the blocks
std::vector<block_t> blocks;

// Array of floats that holds the distances of the blocks around the track (May need to be tweaked if timing motion is unrealistic around the corner).
// Based off of ISU Short Track Special Regulations and Technical Rules.
// TODO: Make track offsets for standing starts, .5 lap starts are 1m apart for 5 tracks or 0.7m for 7 tacks. The furtherest forward track on 500m start line is track 1.   
float blockDistances[14] = {
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

float interBlockDistance = 4.4506;

// track length (Based off of metric distances, 8.5m radius with 28.85m straights)
float trackLength = 111.1071;

// speed profile for auto pacing. Numbers in percentage of top speed (probably needs to be tuned)
float autoPacingDefaultSpeedProfile[9] = {
    0.3241, // Speed at time 0. Starting is very punchy, not good to start at 0% speed.
    0.9000, // Speed at 0.5 laps
    0.9677, // speed at 1 lap
    1.0000, // speed at 1.5 laps

    1.0000, // speed at 2 laps from finish
    0.9890, // speed at 1.5 laps from finish
    0.9783, // speed at 1 lap from finish
    0.9677, // speed at 0.5 laps from finish
    0.9574  // speed at finish
};

settings_t settings = {
    true,       // number of tracks (true = 7, false = 5)
    1,          // current track nunmber
    false,      // starting on 500m side (true if starting on 500m side, flase if starting on 1000m side)
    2,          // number of leading blocks  
    1,          // number of pacing blocks
    2,          // number of trailing blocks
    false       // show all blocks
};

// Initialize the realTimePacing struct
realTimePacing_t realTimePacing = {
    "Stopped", // status
    5,            // laps
    10.0f,         // lapTime
    false         // is_running
};

autoPacing_t autoPacing = {
    "Stopped",
    9.0,
    90.00,
    true,
    false
};

typedef struct Message {
    int type;      // 0 for discovery (scan), 1 for LED color change, 2 for slave block number update
    uint8_t mac[6];    // Master's MAC address (for scan)
    CRGB colour;    // RGB values (for LED color change)
    int number;    // slave block number
} Message;

// Function to convert MAC address from string (like "AA:BB:CC:DD:EE:FF") to byte array
void parseMACAddress(const String& macStr, uint8_t* mac) {
    sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
}

// Function to convert MAC address byte array to string
String macToString(const uint8_t* mac) {
    char buffer[18];
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buffer);
}

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
    String content;
    char buffer[281];
    while (fgets(buffer, sizeof(buffer), file) != nullptr) {
        content += String(buffer);  // Append each line to the content
    }
    fclose(file);  // Close the file

    // Split the content by commas to get individual device entries
    int blockCount = 0;
    int start = 0;
    while (start < content.length() && blockCount < maxBlocks) {
        block_t newBlock;

        int end = content.indexOf(',', start);
        if (end == -1) end = content.length();  // No more commas, process the last entry

        String entry = content.substring(start, end);
        int separatorIndex = entry.indexOf(';');
        
        // Get MAC address and integer value
        String macStr = entry.substring(0, separatorIndex);
        String intStr = entry.substring(separatorIndex + 1);

        // Parse MAC address and integer
        parseMACAddress(macStr, newBlock.mac);
        // check if it's a virtual block:
        if(newBlock.mac[0] != 0x00 && newBlock.mac[1] != 0x00 && newBlock.mac[2] != 0x00 && newBlock.mac[3] != 0x00 && newBlock.mac[4] != 0x00) {
            newBlock.number = intStr.toInt();
            newBlock.colour = CRGB::Black;
            newBlock.blockId = "block" + String(newBlock.number);
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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Send data to MAC: ");
    Serial.print(mac_addr[0], HEX);
    Serial.print(":");
    Serial.print(mac_addr[1], HEX);
    Serial.print(":");
    Serial.print(mac_addr[2], HEX);
    Serial.print(":");
    Serial.print(mac_addr[3], HEX);
    Serial.print(":");
    Serial.print(mac_addr[4], HEX);
    Serial.print(":");
    Serial.print(mac_addr[5], HEX);
    Serial.println();

    if (status == ESP_NOW_SEND_SUCCESS) {
        if(blinking){
            blocks[blinkBlockIdx].status = "Blinking...";
        }
        Serial.println("Delivery Success");
    } else {
        if(blinking){
            blocks[blinkBlockIdx].status = "Disconnected";
        }
        Serial.println("Delivery Fail");
    }
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

void removePeer(uint8_t* peerMAC) {
    if (esp_now_del_peer(peerMAC) == ESP_OK) {
        Serial.println("Peer removed successfully");
    } else {
        Serial.println("Failed to remove peer");
    }
}

// Callback when ESP-NOW message is received
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    const uint8_t *mac_addr = esp_now_info->src_addr;

    Serial.print("Received data from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Optionally process the received data
    Message receivedMessage;
    memcpy(&receivedMessage, data, sizeof(receivedMessage));

    // if block MAC is in blocks
    bool inBlocks = false;
    int idx;
    for(int i=0; i < blocks.size(); i++) {
        if (memcmp(receivedMessage.mac, blocks[i].mac, sizeof(receivedMessage.mac)) == 0) {
            inBlocks = true;
            idx = i;
        }
    }

    if(inBlocks) {
        // if block number from block != block number in blocks
        if(receivedMessage.number != blocks[idx].number) {
            // send ESP-NOW with blocks data for that block
            Message updateMessage;
            updateMessage.type = 2;
            updateMessage.number = blocks[idx].number;

            esp_err_t result = esp_now_send(blocks[idx].mac, (uint8_t *)&updateMessage, sizeof(updateMessage));
    
            if (result == ESP_OK) {
                Serial.println("Update block number (MAC in blocks, wrong block number on block) message sent successfully");
            } else {
                blocks[idx].status = "Disconnected";
                Serial.println("Error sending Update block number (MAC in blocks, wrong block number on block) message");
            }
        }
        blocks[idx].status = "Working";
    // else if blocks size <= 14
    } else if(blocks.size() <= 14) { 
        // add to ESP peer list
        addPeer(receivedMessage.mac);

        block_t newBlock;
        newBlock.status = "Working";
        for(int i=0; i < 6; i++) {
            newBlock.mac[i] = receivedMessage.mac[i];
        }
        newBlock.colour = CRGB::Black;


        // if block number in blocks
        bool numInBlocks = false;
        for(int i=0; i < blocks.size(); i++) {
            if(receivedMessage.number == blocks[i].number) {
                numInBlocks = true;
            }
        }

        if(numInBlocks) {
            // assign block to smallest available number
            for(int i=0; i < blocks.size(); i++) {
                if(blocks[i].number != i+1) {
                    // found a free number
                    newBlock.number = i+1;
                    newBlock.blockId = "block" + String(i+1);
                    break;
                }
                if(i == blocks.size()-1) {
                    // checked all the blocks, they're in order so we assign it to the next (i+2) value.
                    newBlock.number = i+2;
                    newBlock.blockId = "block" + String(i+2);
                }
            }

            // add to blocks
            blocks.push_back(newBlock);
            // send updated number to block
            Message updateMessage;
            updateMessage.type = 2;
            updateMessage.number = newBlock.number;

            esp_err_t result = esp_now_send(newBlock.mac, (uint8_t *)&updateMessage, sizeof(updateMessage));
    
            if (result == ESP_OK) {
                Serial.println("Update Block number (no MAC in blocks, but num in blocks) message sent successfully");
            } else {
                blocks[blocks.size() - 1].status = "Disconnected";
                Serial.println("Error sending update block number (no MAC in blocks, but num in blocks) message");
            }
        // else
        } else {
            // add to blocks with given number
            newBlock.number = receivedMessage.number;
            newBlock.blockId = "block" + String(newBlock.number);
            blocks.push_back(newBlock);
        }
    }

    sendSetupUpdates(); // make sure blocks is ordered
}

void updateLED(int i){
    
    if(i == 0) {
        leds[0] = blocks[0].colour;
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

void updateLEDs(){
    leds[0] = blocks[0].colour;
    FastLED.show();
    if(realTimePacing.is_running){
        sendRealTimePacingUpdates();
    } else if(autoPacing.autoIsRunning){
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

void blinkBlock(String mac, int idx) {
    blinkBlockMac = mac;
    blinkBlockIdx = idx;
    xTaskCreate(&blinkBlockTask, "blinkBlockTask", 4096, NULL, 5, NULL);
}

int ensureRedirect(String path) {
    // Redirect to blocklights.local
    if(reDir) {
        reDir = 0;
        String URL= "http://blocklights.local" + path;
        server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        server.sendHeader("Location", URL, true); // 301 (temporary) or 302 (permanent) redirect
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
    String fullPath = String("/partition/") + filename;

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


// Function to start the Wi-Fi network
void setupWiFi() {

    // Initialize NVS (needed for wifi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Retry NVS initialization
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret); // Check for errors in initialization     

    // Initialize TCP/IP adapter and Wi-Fi event handling
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Creates a default Wi-Fi access point interface
    esp_netif_create_default_wifi_ap();  

    // Initialize the Wi-Fi stack in Station mode (STA)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Disable Wi-Fi power-saving mode to prevent deep sleep (similar to WiFi.setSleep(false))
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // Configure Wi-Fi Access Point settings
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "BlockLights",
            .channel = CHANNEL,
            .authmode = WIFI_AUTH_OPEN, // No password
            .max_connection = 10
        },
    };

    // Set Wi-Fi mode to Access Point + Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    // Use 80 MHz bandwidth to allow better wifi standards in AP + STA Mode
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW80));
    // Set AP config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config)); 

    // Start the Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Delay for 1 second to allow AP configuration to settle
    vTaskDelay(pdMS_TO_TICKS(1000));

    Serial.println("wifi started");

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    Serial.println("espnow started");

     // Register broadcast peer (this is necessary for sending broadcast messages)
    addPeer(broadcastMAC);

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // Start mDNS
    if (!MDNS.begin("blocklights")) {
        printf("Error setting up MDNS responder!\n");
        return;
    }
    printf("mDNS responder started\n");

    // Start the DNS server
    dnsServer.start(53, "*", apIP); // Redirect all DNS queries to the access point IP
    Serial.println("DNS Server started");

    // Initialize WebSocket
    webSocket.begin();
    Serial.println("WebSocket started");
}

// Main task to handle the server and client requests
void serverTask(void* pvParameters) {
    while (true) {
        server.handleClient(); // Handle incoming clients
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield for 10ms to avoid task starvation
    }
}

// Main task to handle the server and client requests
void socketTask(void* pvParameters) {
    while (true) {
        // Handle WebSocket communication
        webSocket.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield for 10ms to avoid task starvation
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

void realTimePacingTask(void* pvParameters) {

    float blueOffset, greenOffset, redOffset, offOffset, trackOffset;
    getOffsets(blueOffset, greenOffset, redOffset, offOffset, trackOffset);
    
    float originalNumLaps = realTimePacing.lap;
    float prevDist = 0.0f;
    
    int64_t pacingStartTime = esp_timer_get_time();
    int64_t prevTime = pacingStartTime;
    bool blocksChanged = false;
    while(realTimePacing.is_running && realTimePacing.lap > 0) {
        int64_t currentTime = esp_timer_get_time();
        int64_t timeDelta = currentTime - prevTime;
        float currentSpeed = trackLength / realTimePacing.lapTime;
        float currentDist = prevDist + (timeDelta/(1000000.0)) * currentSpeed;
        

        // update the lap value if we do a lap
        if(currentDist > trackLength) {
            realTimePacing.lap -= 1;
            currentDist -= trackLength;

            Serial.print("Lap count updated, laps to go: ");
            Serial.println(realTimePacing.lap);
            Serial.print("Lapt time: ");
            Serial.println(realTimePacing.lapTime);
            Serial.print("Time delta (us): ");
            Serial.print(timeDelta);
        }
        
        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {

            
            float blockPosition = blockDistances[blocks[i].number - 1] + trackOffset;

            // edge case for crossing the start/finish lines and track lenght being reset
            if(blockPosition - currentDist > trackLength/2) {
                blockPosition -= trackLength;
            } else if(currentDist - blockPosition > trackLength/2) {
                blockPosition += trackLength;
            }

            if(blockPosition < (currentDist + offOffset)) {
                if(blocks[i].colour != CRGB::Black) {
                    // change block to off
                    blocks[i].colour = CRGB::Black;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour off");
                }
            } else if(blockPosition < currentDist + redOffset) {
                if(blocks[i].colour != CRGB::Red) {
                    // change block to red
                    blocks[i].colour = CRGB::Red;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour red");
                }
            } else if(blockPosition < currentDist + greenOffset) {
                if(blocks[i].colour != CRGB::Green) {
                    // change block to green
                    blocks[i].colour = CRGB::Green;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour green");
                }
            } else if(blockPosition < currentDist + blueOffset) {
                if(blocks[i].colour != CRGB::Blue) {
                    // change block to blue
                    blocks[i].colour = CRGB::Blue;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour blue");
                }
            }
        }

        if(blocksChanged) {
            sendRealTimePacingUpdates();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        blocksChanged = false;
        prevDist = currentDist;
        prevTime = currentTime;

    }

    Serial.println("Pacing finished (or stopped)");

    // Turn all blocks off
    for (int i = 0; i < size(blocks); i++) {
        blocks[i].colour = CRGB::Black;
    }

    updateLEDs();

    // reset the lap count for the next set
    realTimePacing.lap = originalNumLaps;
    // send updates to client?
    realTimePacing.is_running = false;
    realTimePacing.status = "Stopped";
    sendRealTimePacingUpdates();

    
    vTaskDelay(50 / portTICK_PERIOD_MS); // Wait a little bit before deleting the task
    vTaskDelete(NULL);  // Delete this task
}

// Real-time pacing function
void startRealTimePacingTask() {
    autoPacing.autoIsRunning = false;
    xTaskCreate(&realTimePacingTask, "realTimePacingTask", 4096, NULL, 5, NULL);
}


void autoPacingTask(void* pvParameters) {
    float blueOffset, greenOffset, redOffset, offOffset, trackOffset;
    getOffsets(blueOffset, greenOffset, redOffset, offOffset, trackOffset);


    std::vector<float> autoPacingSpeedProfile;
    // Need to get array of speed percentages, per half lap
    for(int i = 0; i <= 2*autoPacing.autoLaps; i++){
        if(i <= 8) { // 9 is the size of autoPacingDefaultSpeedProfile
             autoPacingSpeedProfile.push_back(autoPacingDefaultSpeedProfile[i]);
        } else{
            autoPacingSpeedProfile.insert(autoPacingSpeedProfile.begin() + 4, 1.00); // insert half laps at 1.00 speed after 1.5 laps
        }
       
    }

    // calculate the 100% speed for given time (linear acceleration assumed)
    float temp = 0.0;
    for(int i = 0; i < size(autoPacingSpeedProfile)-1; i++) {
        temp += 1/(autoPacingSpeedProfile[i+1] + autoPacingSpeedProfile[i]);
    }
    float maxSpeed = trackLength*temp/autoPacing.autoTime; // max speed in terms of m/s
    Serial.println("Max speed:");
    Serial.println(maxSpeed);

    // convert percentage max speed values into m/s
    for (int i = 0; i < size(autoPacingSpeedProfile); ++i) {
        autoPacingSpeedProfile[i] = maxSpeed*autoPacingSpeedProfile[i];
    }


    // 5 second countdown
    if(autoPacing.autoCountdown){
        
        Serial.println("Starting countdown...");

        uint64_t countDownStartTime = esp_timer_get_time();
        uint64_t currentTime = countDownStartTime;
        while(autoPacing.autoIsRunning && currentTime - countDownStartTime < 3000000){
            currentTime = esp_timer_get_time();
            if (currentTime - countDownStartTime > 0 && currentTime - countDownStartTime < 500000 && blocks[0].colour != CRGB::Red) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Red;
                }
                updateLEDs();
            } else if (currentTime - countDownStartTime > 500000 && currentTime - countDownStartTime < 1000000 && blocks[0].colour != CRGB::Black) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Black;
                }
                updateLEDs();
            } else if (currentTime - countDownStartTime > 1000000 && currentTime - countDownStartTime < 1500000 && blocks[0].colour != CRGB::Red) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Red;
                }
                updateLEDs();
            } else if (currentTime - countDownStartTime > 1500000 && currentTime - countDownStartTime < 2000000 && blocks[0].colour != CRGB::Black) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Black;
                }
                updateLEDs();
            } else if (currentTime - countDownStartTime > 2000000 && currentTime - countDownStartTime < 2500000 && blocks[0].colour != CRGB::Orange) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Orange;
                }
                updateLEDs();
            } else if (currentTime - countDownStartTime > 2500000 && blocks[0].colour != CRGB::Black) {
                for (int i = 0; i < size(blocks); i++) {
                    blocks[i].colour = CRGB::Black;
                }
                updateLEDs();
            }
            vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        }
        for (int i = 0; i < size(blocks); i++) {
            blocks[i].colour = CRGB::Green;
        }
        updateLEDs();
        Serial.println("Finishing countdown");
    }

    if(!autoPacing.autoIsRunning){
        Serial.println("Pacing finished (or stopped)");

        // Turn all blocks off
        for (int i = 0; i < size(blocks); i++) {
            blocks[i].colour = CRGB::Black;
        }
        updateLEDs();
        autoPacing.autoStatus = String("Stopped");
        sendAutoPacingUpdates();
        vTaskDelay(50 / portTICK_PERIOD_MS); // let other tasks run, needed to finish sending autopacing updates
        vTaskDelete(NULL);  // Delete this task
    }
    

    // calculate the total distance:
    float totalDist = autoPacing.autoLaps * trackLength;
    int originalNumLaps = floor(autoPacing.autoLaps);

    // Start AutoPacing!
    int numHalfLaps = 0;
    float currentDist = 0.0;
    float prevLaptime = 0.0;

    Serial.println("Started AutoPacing");
    int64_t pacingStartTime = esp_timer_get_time();
    int64_t prevHalfLapTime = pacingStartTime;

    if(autoPacing.autoCountdown){   // wait for 1 second of green
        while(esp_timer_get_time() - pacingStartTime < 1000000){
            vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        }

        // Turn all blocks off
        for (int i = 0; i < size(blocks); i++) {
            blocks[i].colour = CRGB::Black;
        }
        updateLEDs();
    }


    bool blocksChanged = false;
    while(autoPacing.autoIsRunning && currentDist < totalDist) {
        int64_t currentTime = esp_timer_get_time();

        float sPrev = autoPacingSpeedProfile[numHalfLaps]; // m/s speed of the previous half lap checkpoint
        float sNext = autoPacingSpeedProfile[numHalfLaps+1]; // m/s speed of the next half lap checkpoint

        float currentSpeed = (sNext*sNext - sPrev*sPrev)*(currentTime - prevHalfLapTime)/(trackLength*1000000) + sPrev; 

        float currentLaptime = trackLength/currentSpeed;

        if(fabs(currentLaptime - prevLaptime) > 0.01) {
            // update the status to show the current speed
            autoPacing.autoStatus = String("Running. Laptime: ") + String(currentLaptime, 2);
            prevLaptime = currentLaptime;

            // update the webserver 
            sendAutoPacingUpdates(); 
        }

        currentDist = numHalfLaps*trackLength/2 + (currentTime - prevHalfLapTime)*(currentSpeed + sPrev)/2000000;
        
        // update previous half lap time. i is the number of half laps (NEEDS to be after line above, otherwise ends in a loop because prevHalfLaptTime is calculated after currentTime.)
        if(2*currentDist > (numHalfLaps+1)*trackLength){
            numHalfLaps++;
            prevHalfLapTime = currentTime;
        }

        // update lap count
        if(int((2*autoPacing.autoLaps))%2 == 1 && 2*currentDist > trackLength ){
            autoPacing.autoLaps -= 0.5;
            Serial.print("Lap count updated, laps to go: ");
            Serial.println(autoPacing.autoLaps);
            sendAutoPacingUpdates();
        } else if(currentDist > (originalNumLaps - autoPacing.autoLaps + 1)*trackLength + trackLength*settings.startingOn500mSide/2){
            Serial.print("Total distance: ");
            Serial.println(currentDist);
            autoPacing.autoLaps -= 1;
            Serial.print("Lap count updated, laps to go: ");
            Serial.println(autoPacing.autoLaps);
            sendAutoPacingUpdates();
        }

        // calculate current distance around the track (start/finish line is exactly 0m)
        float lapDist = currentDist + trackLength*settings.startingOn500mSide/2;
        while(lapDist > trackLength){
            lapDist -= trackLength;
        }

        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {

            float blockPosition = blockDistances[blocks[i].number - 1] + trackOffset;

            // edge case for crossing the start/finish lines and track length being reset
            if(blockPosition - lapDist > trackLength/2) {
                blockPosition -= trackLength;
            } else if(lapDist - blockPosition > trackLength/2) {
                blockPosition += trackLength;
            }

            if(blockPosition < (lapDist + offOffset)) {
                if(blocks[i].colour != CRGB::Black) {
                    // change block to off
                    blocks[i].colour = CRGB::Black;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour off");
                }
            } else if(blockPosition < lapDist + redOffset) {
                if(blocks[i].colour != CRGB::Red) {
                    // change block to red
                    blocks[i].colour = CRGB::Red;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour red");
                }
            } else if(blockPosition < lapDist + greenOffset) {
                if(blocks[i].colour != CRGB::Green) {
                    // change block to green
                    blocks[i].colour = CRGB::Green;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour green");
                }
            } else if(blockPosition < lapDist + blueOffset) {
                if(blocks[i].colour != CRGB::Blue) {
                    // change block to blue
                    blocks[i].colour = CRGB::Blue;
                    updateLED(i);
                    blocksChanged = true;
                    Serial.println("Changed block " + String(i+1) + " to colour blue");
                }
            }
        }

        if(blocksChanged) {
            sendAutoPacingUpdates();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        blocksChanged = false;
    }
    Serial.print("TOTAL TIME TAKEN: ");
    Serial.println(String((esp_timer_get_time() - pacingStartTime)/1000000.0, 3));
    Serial.println(autoPacing.autoTime);

    Serial.println("Pacing finished (or stopped)");

    // Turn all blocks off
    for (int i = 0; i < size(blocks); i++) {
        blocks[i].colour = CRGB::Black;
    }
    updateLEDs();

    // send updates to client
    autoPacing.autoLaps = originalNumLaps;
    autoPacing.autoIsRunning = false;
    autoPacing.autoStatus = "Stopped";
    sendAutoPacingUpdates();

    vTaskDelay(50 / portTICK_PERIOD_MS); // let other tasks run, needed to finish sending autopacing updates
    
    vTaskDelete(NULL);  // Delete this task
}


void startAutoPacingTask() {
    realTimePacing.is_running = false;
    if(int(2*autoPacing.autoLaps) % 2 == 0) {
        settings.startingOn500mSide = false;
    } else {
        settings.startingOn500mSide = true;
    }

    xTaskCreate(&autoPacingTask, "autoPacingTask", 4096, NULL, 5, NULL);

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
            block_t block = { {0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)i}, 0, "Virtual", i, "block" + String(i), CRGB::Black};
            blocks.push_back(block);
        } 
    }
    sendSetupUpdates();
}

void killPacingTasks(){
    realTimePacing.is_running = false;
    autoPacing.autoIsRunning = false;

    // turn blocks off
    vTaskDelay(pdMS_TO_TICKS(1000));
    for(int i = 0; i < blocks.size(); i++){
        blocks[i].colour = CRGB::Black;
    }
    updateLEDs();
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


// Main function for initializing peripherals and starting tasks
extern "C" void app_main(void) {

    //esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error  WARNING setting all logs to just error BREAKS FASTLED (3.8 prerelease)

    Serial.begin(115200); // Initialize Serial communication

    //delay(5000); // 5 seconds to let you open the COM port if you need

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/partition",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    // Setup Wi-Fi and LED control
    printf("\nInitialising......\n");
    setupWiFi();

    // Serve CSS
    server.on("/style.css", []() {
        serveFile("/styles/style.css", "text/css");
    });

    // Serve the main index page
    server.on("/", []() {
        killPacingTasks();
        serveFile("/home.html", "text/html");
    });

    // Initialise routes for the Real-Time Pacing
    initRealTimePacingRoutes(server);

    // initialise routes for the settings page
    initSettingsRoutes(server);

    // initialise routes for the auto pacing page
    initAutoPacingRoutes(server);

    // initialise routes for the auto pacing page
    initSetupRoutes(server);

    // Serve the favicon.ico file
    server.on("/favicon.ico", HTTP_GET, []() {
        serveFile("/favicon.ico", "image/x-icon");
    });

    // Redirect any other request to blocklights.local
    server.onNotFound([]() {
        server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
        server.send(302, "text/plain", "Redirecting to blocklights.local");
    });

    server.begin();                      // Start the web server
    printf("Server started\n");

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, masterMacAddress));
    masterMacAddressString = macToString(masterMacAddress);
    Serial.println(masterMacAddressString);
   
    // read blocks data from nv memory
    readBlocksFromFile();

    // add to ESP peers list
    for(int i=0; i < blocks.size(); i++) {
        addPeer(blocks[i].mac);
    }

    block_t masterBlock = {{masterMacAddress[0], masterMacAddress[1], masterMacAddress[2], 
        masterMacAddress[3], masterMacAddress[4], masterMacAddress[5]}, 0, "Master", 1, "block1", CRGB::Black};

    blocks.insert(blocks.begin(), masterBlock);

    // scan blocks for number (maybe two or three times), also orders them
    scanForBlocks();
    //addAllBlocks(); // Add the other 13 blocks

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED
    updateLEDs(); // Make sure all the LEDs are off.

    // Create the server task - runs continuously and checks for http requests from clients
    xTaskCreate(&serverTask, "serverTask", 4096, NULL, 5, NULL);
    xTaskCreate(&socketTask, "socketTask", 4096, NULL, 5, NULL);

}





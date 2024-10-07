#include <WiFi.h>
#include <WebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nvs_flash.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <FastLED.h>


//Custom libraries
#include <realTimePacing.h>
#include <blockLightsSettings.h>
#include <autoPacing.h>


#define NUM_LEDS 1        // Number of LEDs in your strip
#define DATA_PIN 8         // Pin connected to your LED strip

CRGB leds[NUM_LEDS];


const char* ssid = "BlockLights";   // SSID for the Access Point

// DNS server
DNSServer dnsServer;

WebServer server(80);                 // Create a web server on port 80
WebSocketsServer webSocket = WebSocketsServer(81);  // Create WebSocket on port 81

bool reDir = 1;

IPAddress apIP(192, 168, 4, 1);

// Struct for each of the blocks
typedef struct {
    uint8_t mac[6];      // MAC address of the slave
    int rssi;            // Signal strength
    String status;         // Online/offline or active status
    int number;      // number of block along track
    CRGB colour;
} block_t;

// Array of structs to hold all the blocks
std::vector<block_t> blocks;

// Array of floats that holds the distances of the blocks around the track (May need to be tweaked if timing motion is unrealistic around the corner).
// Based off of ISU Short Track Special Regulations and Technical Rules.
// TODO: Make track offsets for standing starts, .5 lap starts are 1m apart for 5 tracks or 0.7m for 7 tacks. The furtherest forward track on 500m start line is track 1.   
float blockDistances[14] = {
    14.4250,  // First block (master block)
    18.8756,
    23.3262,
    27.7768, // Apex blcok of first corner
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
    2           // number of trailing blocks
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

void updateLEDs(){
    leds[0] = blocks[0].colour;
    FastLED.show();
    // SEND ESP NOW COMMAND
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
    if (SPIFFS.exists(filename)) {
        File file = SPIFFS.open(filename, "r");
        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "File Not Found");
    }
}

// Function to start the Wi-Fi network
void setupWiFi() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);  // Ensure NVS is initialized successfully

    // Set up access point with a specific channel and maximum connection
    WiFi.softAP(ssid, "", 11);          // wifi name, password, channel
    printf("Access Point Started\n");
    
    // Wait a moment for the AP to configure itself
    delay(1000);
    
    // Check IP address
    IPAddress IP = WiFi.softAPIP();
    printf("IP Address: %d.%d.%d.%d\n", IP[0], IP[1], IP[2], IP[3]); // Should print the actual IP address, not 0.0.0.0

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

            // update the webserver 
            sendRealTimePacingUpdates();
        }
        
        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {

            
            float blockPosition = blockDistances[blocks[i].number] + trackOffset;

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
                    Serial.println("Changed block " + String(i+1) + " to colour off");
                }
            } else if(blockPosition < currentDist + redOffset) {
                if(blocks[i].colour != CRGB::Red) {
                    // change block to red
                    blocks[i].colour = CRGB::Red;
                    Serial.println("Changed block " + String(i+1) + " to colour red");
                }
            } else if(blockPosition < currentDist + greenOffset) {
                if(blocks[i].colour != CRGB::Green) {
                    // change block to green
                    blocks[i].colour = CRGB::Green;
                    Serial.println("Changed block " + String(i+1) + " to colour green");
                }
            } else if(blockPosition < currentDist + blueOffset) {
                if(blocks[i].colour != CRGB::Blue) {
                    // change block to blue
                    blocks[i].colour = CRGB::Blue;
                    Serial.println("Changed block " + String(i+1) + " to colour blue");
                }
            }
        }

        // Send colour updates to the LEDs
        updateLEDs();

        vTaskDelay(1 / portTICK_PERIOD_MS); // let other tasks run
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
    sendRealTimePacingUpdates();

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

    // calculate the 100% speed for given time
    float temp = 0.0;
    for(int i = 0; i < size(autoPacingSpeedProfile)-1; i++) {
        temp += 1/(autoPacingSpeedProfile[i+1] + autoPacingSpeedProfile[i]);
    }
    float maxSpeed = autoPacing.autoTime/temp; // max speed in terms of laptimes
    Serial.println("Max speed:");
    Serial.println(maxSpeed);

    // convert percentage max speed values into laptimes.
    Serial.println("Auto Pacing Speed Profile:");
    for (int i = 0; i < size(autoPacingSpeedProfile); ++i) {
        autoPacingSpeedProfile[i] = maxSpeed/autoPacingSpeedProfile[i];
        Serial.println(autoPacingSpeedProfile[i]);
    }
    
    float originalNumLaps = autoPacing.autoLaps;  
    float prevDist;
    if(int((2*autoPacing.autoLaps)) % 2 == 0) {
        prevDist = 0.0f;
    } else {
        prevDist = trackLength/2; 
        autoPacing.autoLaps += 0.5; 
    }
    float prevLapTime = 0.0;


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


            vTaskDelay(1 / portTICK_PERIOD_MS); // let other tasks run
        }
        

        for (int i = 0; i < size(blocks); i++) {
            blocks[i].colour = CRGB::Green;
        }
        updateLEDs();

        Serial.println("Finishing countdown");
        

    }


    
    int64_t pacingStartTime = esp_timer_get_time();
    int64_t prevTime = pacingStartTime;
    while(autoPacing.autoIsRunning && autoPacing.autoLaps > 0) {
        int64_t currentTime = esp_timer_get_time();
        int64_t timeDelta = currentTime - prevTime;

        // calculate laptime...
        int i = 2*(originalNumLaps - autoPacing.autoLaps);
        float lapTime;
        float vInitial;
        float vFinal;
        if(prevDist > 0.5*trackLength) {
            i += 1;
            vInitial = autoPacingSpeedProfile[i];
            vFinal = autoPacingSpeedProfile[i+1];
            lapTime = (vFinal - vInitial)*(2*prevDist/trackLength - 1) + vInitial;
        } else {
            vInitial = autoPacingSpeedProfile[i];
            vFinal = autoPacingSpeedProfile[i+1];
            lapTime = (vFinal - vInitial)*(2*prevDist/trackLength) + vInitial;
        }

        if(fabs(lapTime - prevLapTime) > 0.01) {
            // update the status to show the current speed?
            autoPacing.autoStatus = String("Running. Laptime: ") + String(lapTime, 2);
            prevLapTime = lapTime;

            // update the webserver 
            sendAutoPacingUpdates();
        }

        float currentSpeed = trackLength / lapTime;
        float currentDist = prevDist + (timeDelta/(1000000.0)) * currentSpeed;

        // update the lap value if we do a lap
        if(currentDist > trackLength) {
            autoPacing.autoLaps -= 1;
            currentDist -= trackLength;

            Serial.print("Lap count updated, laps to go: ");
            Serial.println(autoPacing.autoLaps);
            Serial.print("Lap time: ");
            Serial.println(lapTime);
            Serial.print("Time delta (us): ");
            Serial.println(timeDelta);
        }
        
        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {

            
            float blockPosition = blockDistances[blocks[i].number] + trackOffset;

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
                    Serial.println("Changed block " + String(i+1) + " to colour off");
                }
            } else if(blockPosition < currentDist + redOffset) {
                if(blocks[i].colour != CRGB::Red) {
                    // change block to red
                    blocks[i].colour = CRGB::Red;
                    Serial.println("Changed block " + String(i+1) + " to colour red");
                }
            } else if(blockPosition < currentDist + greenOffset) {
                if(blocks[i].colour != CRGB::Green) {
                    // change block to green
                    blocks[i].colour = CRGB::Green;
                    Serial.println("Changed block " + String(i+1) + " to colour green");
                }
            } else if(blockPosition < currentDist + blueOffset) {
                if(blocks[i].colour != CRGB::Blue) {
                    // change block to blue
                    blocks[i].colour = CRGB::Blue;
                    Serial.println("Changed block " + String(i+1) + " to colour blue");
                }
            }
        }

        if((autoPacing.autoCountdown && (currentTime - pacingStartTime) > 1000000) || !autoPacing.autoCountdown) {
            updateLEDs();
        }


        vTaskDelay(1 / portTICK_PERIOD_MS); // let other tasks run
        prevDist = currentDist;
        prevTime = currentTime;

    }

    Serial.println("Pacing finished (or stopped)");

    // Turn all blocks off
    for (int i = 0; i < size(blocks); i++) {
        blocks[i].colour = CRGB::Black;
    }

    updateLEDs();


    // send updates to client?
    autoPacing.autoLaps = originalNumLaps;
    autoPacing.autoIsRunning = false;
    autoPacing.autoStatus = String("Stopped");
    sendAutoPacingUpdates();
    

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


// Main function for initializing peripherals and starting tasks
extern "C" void app_main(void) {

    //esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error  WARNING THIS BREAKS FASTLED (3.8 prerelease)  

    Serial.begin(115200); // Initialize Serial communication

    //delay(5000); // 5 seconds to let you open the COM port if you need

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        return;
    }

    // Setup Wi-Fi and LED control
    printf("\nInitialising......\n");
    setupWiFi();

    // Serve CSS
    server.on("/style.css", []() {
        serveFile("/styles/style.css", "text/css");
    });

    // Serve the main index page
    server.on("/", []() {
        serveFile("/home.html", "text/html");
    });

    // Initialise routes for the Real-Time Pacing
    initRealTimePacingRoutes(server);

    // initialise routes for the settings page
    initSettingsRoutes(server);

    // initialise routes for the auto pacing page
    initAutoPacingRoutes(server);

    // Serve the favicon.ico file
    server.on("/favicon.ico", HTTP_GET, []() {
        if (SPIFFS.exists("/favicon.ico")) {
            File file = SPIFFS.open("/favicon.ico", "r");
            server.streamFile(file, "image/x-icon");
            file.close();
        } else {
            server.send(404, "text/plain", "404: Favicon not found");
        }
    });

    // Redirect any other request to blocklights.local
    server.onNotFound([]() {
        server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
        server.send(302, "text/plain", "Redirecting to blocklights.local");
    });

    server.begin();                      // Start the web server
    printf("Server started\n");

    block_t newBlock = { {0x30, 0xAE, 0xA4, 0x45, 0x67, 0x89}, 0, "working", 1, CRGB::Black};
    blocks.push_back(newBlock);

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED (This needs to be first or it won't work)
    updateLEDs(); // Make sure all the LEDs are off.

    // Create the server task - runs continuously and checks for http requests from clients
    xTaskCreate(&serverTask, "serverTask", 4096, NULL, 5, NULL);
    xTaskCreate(&socketTask, "socketTask", 4096, NULL, 5, NULL);

}





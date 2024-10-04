#include <WiFi.h>
#include <WebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nvs_flash.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>

//Custom libraries
#include <realTimePacing.h>
#include <blockLightsSettings.h>
#include <autoPacing.h>

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
    String colour;
} block_t;

// Array of structs to hold all the blocks
std::vector<block_t> blocks;

// Array of floats that holds the distances of the blocks around the track (May need to be tweaked if timing motion is unrealistic around the corner).
// Based off of ISU Short Track Special Regulations and Technical Rules.
// TODO: Make track offsets for standing starts, .5 lap starts are 1m apart for 5 tracks or 0.7m for 7 tacks. The furtherest forward track on 500m start line is track 1.   
std::vector<float> blockDistances = {
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

// track length (Based off of metric distances, 8.5m radius with 28.85m straights)
float trackLength = 111.1071;

settings_t settings = {
    true,       // number of tracks (true = 7, false = 5)
    1,          // current track nunmber
    false       // starting on 500m side (true if starting on 500m side, flase if starting on 1000m side)
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

void realTimePacingTask(void* pvParameters) {

    // These parameters should be changeable in a settings page
    int numLeadingBlocks = 3;
    int numPacingBlocks = 1;
    int numTrailingBlocks = 3;
    float interBlockistance = 4.4506;

    // figure out distance offset from pacing distance to each light
    // turn blue distance in front
    float blueOffset = ((numPacingBlocks-1)/2.0  + numLeadingBlocks) * interBlockistance;
    // turn green distance in front
    float greenOffset = ((numPacingBlocks-1)/2.0) * interBlockistance;
    // turn red distance behind
    float redOffset = -(((numPacingBlocks-1)/2.0 + 1) * interBlockistance);
    // turn off distance behind
    float offOffset = -(((numPacingBlocks-1)/2.0 + 1 + numTrailingBlocks) * interBlockistance);

    // Vary the offsets based on which track we are on:
    float trackOffset;
    if(settings.numberOfTracks) {
        trackOffset = (settings.trackNumber - 4)*0.7*(1.0 + settings.startingOn500mSide);
    } else {
        trackOffset = (settings.trackNumber - 3)*(1.0 + settings.startingOn500mSide);
    }    
    
    int originalNumLaps = realTimePacing.lap;  
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
                if(blocks[i].colour != "Off") {
                    // change block to off
                    blocks[i].colour = "Off";
                    Serial.println("Changed block " + String(i+1) + " to colour off");
                }
            } else if(blockPosition < currentDist + redOffset) {
                if(blocks[i].colour != "Red") {
                    // change block to red
                    blocks[i].colour = "Red";
                    Serial.println("Changed block " + String(i+1) + " to colour red");
                }
            } else if(blockPosition < currentDist + greenOffset) {
                if(blocks[i].colour != "Green") {
                    // change block to green
                    blocks[i].colour = "Green";
                    Serial.println("Changed block " + String(i+1) + " to colour green");
                }
            } else if(blockPosition < currentDist + blueOffset) {
                if(blocks[i].colour != "Blue") {
                    // change block to blue
                    blocks[i].colour = "Blue";
                    Serial.println("Changed block " + String(i+1) + " to colour blue");
                }
            }
        }

        // ESPNOW the colour data to all the blocks


        vTaskDelay(1 / portTICK_PERIOD_MS); // let other tasks run
        prevDist = currentDist;
        prevTime = currentTime;

    }

    Serial.println("Pacing finished (or stopped)");
    // reset the lap count for the next set
    delay(2000);
    realTimePacing.lap = originalNumLaps;

    vTaskDelete(NULL);  // Delete this task
}

// Real-time pacing function
void startRealTimePacingTask() {

    xTaskCreate(&realTimePacingTask, "realTimePacingTask", 4096, NULL, 5, NULL);
}


// void autoPacingTask(){}

void startAutoPacingTask() {

    //xTaskCreate(&realTimePacingTask, "realTimePacingTask", 4096, NULL, 5, NULL);
}


// Main function for initializing peripherals and starting tasks
extern "C" void app_main(void) {
    esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error

    Serial.begin(115200); // Initialize Serial communication

    //delay(5000); // 5 seconds to let you open the COM port if you need

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        return;
    }

    pinMode(LED_BUILTIN, OUTPUT);
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





    block_t newBlock = { {0x30, 0xAE, 0xA4, 0x45, 0x67, 0x89}, 0, "working", 1, "Off"};
    blocks.push_back(newBlock);






    // Create the server task - runs continuously and checks for http requests from clients
    xTaskCreate(&serverTask, "serverTask", 4096, NULL, 5, NULL);
    xTaskCreate(&socketTask, "socketTask", 4096, NULL, 5, NULL);
}





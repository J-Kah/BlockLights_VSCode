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


#include <ESPmDNS.h> // arduino
#include <DNSServer.h> // arduino
// Hopefully the above can be replaced with ESPAsyncWebServer or similar.

#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_littlefs.h>

// Custom libraries
#include "sharedData.h"
#include "realTimePacing.h"
#include "blockLightsSettings.h"
#include "autoPacing.h"
#include "blockLightsSetup.h"

// DNS server
static DNSServer dnsServer;

WebSocketsServer webSocket = WebSocketsServer(81);  // Create WebSocket on port 81

static const IPAddress apIP(192, 168, 4, 1);
const uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast address for scan
uint8_t masterMacAddress[6];
static std::string masterMacAddressString;

// Callback when ESP-NOW message is sent
static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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

// Callback when ESP-NOW message is received
static void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
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
            updateMessage.type = SLAVE_BLOCK_UPDATE;
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
                    newBlock.blockId = "block" + std::to_string(i+1);
                    break;
                }
                if(i == blocks.size()-1) {
                    // checked all the blocks, they're in order so we assign it to the next (i+2) value.
                    newBlock.number = i+2;
                    newBlock.blockId = "block" + std::to_string(i+2);
                }
            }

            // add to blocks
            blocks.push_back(newBlock);
            // send updated number to block
            Message updateMessage;
            updateMessage.type = SLAVE_BLOCK_UPDATE;
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
            newBlock.blockId = "block" + std::to_string(newBlock.number);
            blocks.push_back(newBlock);
        }
    }

    sendSetupUpdates(); // make sure blocks is ordered
}

// Function to start the Wi-Fi network
static void setupWiFi() {

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
    addPeer(const_cast<uint8_t*>(broadcastMAC));

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
static void serverTask(void* pvParameters) {
    while (true) {
        server.handleClient(); // Handle incoming clients
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield for 10ms to avoid task starvation
    }
}

// Main task to handle the server and client requests
static void socketTask(void* pvParameters) {
    while (true) {
        // Handle WebSocket communication
        webSocket.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield for 10ms to avoid task starvation
    }
}

static void initServerRoutes() {
    // Serve CSS
    server.on("/style.css", []() {
        serveFile("/styles/style.css", "text/css");
    });

    // Serve the main index page
    server.on("/", []() {
        killPacingTasks(realTimePacing, autoPacing);
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
}

static void initLittleFS() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/partition",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));
}

static void addESPPeers() {
    // add to ESP peers list
    for(int i=0; i < blocks.size(); i++) {
        addPeer(blocks[i].mac);
    }

    block_t masterBlock = {{masterMacAddress[0], masterMacAddress[1], masterMacAddress[2], 
        masterMacAddress[3], masterMacAddress[4], masterMacAddress[5]}, 0, "Master", 1, "block1", CRGB::Black};

    blocks.insert(blocks.begin(), masterBlock);

    // scan blocks for number (maybe two or three times), also orders them
    scanForBlocks();
}

// Main function for initializing peripherals and starting tasks
extern "C" void app_main(void) {

    //esp_log_level_set("*", ESP_LOG_ERROR);  // Set all log levels to error  WARNING setting all logs to just error BREAKS FASTLED (3.8 prerelease)

    Serial.begin(115200); // Initialize Serial communication

    //delay(5000); // 5 seconds to let you open the COM port if you need

    initLittleFS();

    // Setup Wi-Fi and LED control
    printf("\nInitialising......\n");
    setupWiFi();

    initServerRoutes();

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, masterMacAddress));
    masterMacAddressString = macToString(masterMacAddress);
    Serial.println(masterMacAddressString.c_str());
   
    // read blocks data from nv memory
    readBlocksFromFile();

    addESPPeers();

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize FastLED
    updateLEDs(); // Make sure all the LEDs are off.

    // Create the server task - runs continuously and checks for http requests from clients
    xTaskCreate(&serverTask, "serverTask", 4096, NULL, 5, NULL);
    xTaskCreate(&socketTask, "socketTask", 4096, NULL, 5, NULL);

}





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

const char* ssid = "BlockLights";   // SSID for the Access Point

// DNS server
DNSServer dnsServer;

WebServer server(80);                 // Create a web server on port 80
WebSocketsServer webSocket = WebSocketsServer(81);  // Create WebSocket on port 81

bool reDir = 1;

IPAddress apIP(192, 168, 4, 1);


// Initialize the realTimePacing struct
realTimePacing_t realTimePacing = {
    "Stopped", // status
    5,            // laps
    10.0f,         // lapTime
    false         // is_running
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
    WiFi.softAP(ssid);
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

    // Handle the captive portal requests
    // server.on("/generate_204", []() {
    //     server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
    //     server.send(302, "text/plain", "Redirecting to blocklights.local");
    // });
    // // I could just delete line 793 in WebServer.cpp but I hate touching source code
    // server.on("/gen_204", []() {
    //     server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
    //     server.send(302, "text/plain", "Redirecting to blocklights.local");
    // });
    // server.on("/chat", []() {
    //     server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
    //     server.send(302, "text/plain", "Redirecting to blocklights.local");
    // });
    // server.on("/connecttest.txt", []() {
    //     server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
    //     server.send(302, "text/plain", "Redirecting to blocklights.local");
    // });
    // server.on("/redirect", []() {
    //     server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
    //     server.send(302, "text/plain", "Redirecting to blocklights.local");
    // });



    // Redirect any other request to blocklights.local
    server.onNotFound([]() {
        server.sendHeader("Location", "http://blocklights.local", true); // 302 redirect
        server.send(302, "text/plain", "Redirecting to blocklights.local");
    });

    server.begin();                      // Start the web server
    printf("Server started\n");

    // Create the server task - runs continuously and checks for http requests from clients
    xTaskCreate(&serverTask, "serverTask", 4096, NULL, 5, NULL);
    xTaskCreate(&socketTask, "socketTask", 4096, NULL, 5, NULL);
}





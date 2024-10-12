#include "realTimePacing.h"
#include <WebSocketsServer.h>
#include <FastLED.h>

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

// Function to reset realTimePacing to initial values
void resetRealTimePacing() {
    realTimePacing.status = "Stopped";
    realTimePacing.lap = 5;
    realTimePacing.lapTime = 10.0;
    realTimePacing.is_running = false;
}

String CRGBtoHexString2(const CRGB& colour) {
    if(colour == CRGB::Red){
        return("#FF0000");
    }
    if(colour == CRGB::Green){
        return("#00FF00");
    }
    if(colour == CRGB::Blue){
        return("#0000FF");
    }
    if(colour == CRGB::Orange){
        return("#FFA500");
    }
    return ("#000000");
}

void sendRealTimePacingUpdates() {
    // Serial.println(realTimePacing.status);
    // Serial.println(realTimePacing.lap);
    // Serial.println(realTimePacing.lapTime);

    String json = "{";
    json += "\"status\": \"" + realTimePacing.status + "\",";  // status is a string
    json += "\"lap\": \"" + String(realTimePacing.lap) + "\",";  // lap is a number, convert to String
    json += "\"lapTime\": \"" + String(realTimePacing.lapTime, 1) + "\",";  // lapTime is a number convert to String
    json += "\"circles\": [";

    for (size_t i = 0; i < blocks.size(); ++i) {
        json += "{";
        json += "\"id\": \"" + blocks[i].blockId + "\",";  // blockId is a string
        json += "\"color\": \"" + CRGBtoHexString2(blocks[i].colour) + "\"";  // color is a string
        json += "}";

        if (i < blocks.size() - 1) {
            json += ",";
        }
    }

    json += "]";
    json += "}";

    // Send it to all connected WebSocket clients
    webSocket.broadcastTXT(json);
}



// Define the initialization function
void initRealTimePacingRoutes(WebServer &server) {

    // Routes for fetching Real-Time Pacing values
    server.on("/getLap", [&]() {
        server.send(200, "text/plain", String(realTimePacing.lap)); // Send lap value as plain text
    });

    // Route to get lap time
    server.on("/getLapTime", [&]() {
        server.send(200, "text/plain", String(realTimePacing.lapTime, 1)); // Send lap time with 1 decimal place
    });

    // Route to get running status
    server.on("/getStatus", [&]() {
        server.send(200, "text/plain", realTimePacing.status); // Send running status
    });

    server.on("/toggleSystem", [&]() {
        // Turn on or off the running status
        realTimePacing.is_running = !realTimePacing.is_running;
        if(realTimePacing.is_running) {
            startRealTimePacingTask();
            realTimePacing.status = "Running";
        } else {
            realTimePacing.status = "Stopped";
        }

        server.send(200, "text/plain", realTimePacing.status);
    });

    // Serve the Real-Time Pacing page
    server.on("/realTimePacing", []() {
        if(ensureRedirect("/realTimePacing")) {
            return;
        }else {
            killPacingTasks();
            serveFile("/realTimePacing.html", "text/html");
        }
    });

    // Serve Real-Time Pacing JavaScript
    server.on("/realTimePacing.js", []() {
        serveFile("/scripts/realTimePacing.js", "application/javascript");
    });

    // Routes for updating Real-Time Pacing values (no page reloads)
    server.on("/incrementLap", [&]() {
        // Increment laps (to be handled in JS)
        realTimePacing.lap += 1;
        server.send(200, "text/plain", String(realTimePacing.lap));
    });

    server.on("/decrementLap", [&]() {
        // Decrement laps
        if(realTimePacing.lap>1) {
            realTimePacing.lap -= 1;
        }
        
        server.send(200, "text/plain", String(realTimePacing.lap));
    });

    server.on("/incrementLapTime", [&]() {
        // Increment laps (to be handled in JS)
        realTimePacing.lapTime += 0.1;
        server.send(200, "text/plain", String(realTimePacing.lapTime, 1));
    });

    server.on("/decrementLapTime", [&]() {
        // Decrement laps
        if(realTimePacing.lapTime>7.0) {
            realTimePacing.lapTime -= 0.1;
        }

        server.send(200, "text/plain", String(realTimePacing.lapTime, 1));
    });

    // Route to handle reset button press
    server.on("/resetRealTimePacing", [&]() {
        resetRealTimePacing(); // Reset the struct values
        sendRealTimePacingUpdates();
        server.send(200, "text/plain", "Pacing data reset");
    });

}
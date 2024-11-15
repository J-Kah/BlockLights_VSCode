#include "realTimePacing.h"

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

realTimePacingClass realTimePacing = realTimePacingClass("Stopped", 5, false, 10.0f);


// Function to reset realTimePacing to initial values
void resetRealTimePacing() {
    realTimePacing.status = "Stopped";
    realTimePacing.laps = 5;
    realTimePacing.isRunning = false;
    realTimePacing.lapTime = 10.0f;
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
    // Serial.println(realTimePacing.laps);
    // Serial.println(realTimePacing.lapTime);

    String json = "{";
    json += "\"status\": \"" + String(realTimePacing.status.c_str()) + "\",";  // status is a string
    json += "\"lap\": \"" + String(realTimePacing.laps, 0) + "\",";  // lap is a float, convert to String
    json += "\"lapTime\": \"" + String(realTimePacing.lapTime, 1) + "\",";  // lapTime is a number convert to String
    json += "\"circles\": [";

    for (size_t i = 0; i < blocks.size(); ++i) {
        json += "{";
        json += "\"id\": \"" + String(blocks[i].blockId.c_str()) + "\",";  // blockId is a string
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
        server.send(200, "text/plain", String(realTimePacing.laps, 0)); // Send lap value as plain text
    });

    // Route to get lap time
    server.on("/getLapTime", [&]() {
        server.send(200, "text/plain", String(realTimePacing.lapTime, 1)); // Send lap time with 1 decimal place
    });

    // Route to get running status
    server.on("/getStatus", [&]() {
        server.send(200, "text/plain", realTimePacing.status.c_str()); // Send running status
    });

    server.on("/toggleSystem", [&]() {
        // Turn on or off the running status
        realTimePacing.isRunning = !realTimePacing.isRunning;
        if(realTimePacing.isRunning) {
            startRealTimePacingTask();
            realTimePacing.status = "Running";
        } else {
            realTimePacing.status = "Stopped";
        }

        server.send(200, "text/plain", realTimePacing.status.c_str());
    });

    // Serve the Real-Time Pacing page
    server.on("/realTimePacing", []() {
        if(ensureRedirect("/realTimePacing")) {
            return;
        }else {
            killPacingTasks(realTimePacing, autoPacing);
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
        realTimePacing.laps += 1;
        server.send(200, "text/plain", String(realTimePacing.laps, 0));
    });

    server.on("/decrementLap", [&]() {
        // Decrement laps
        if(realTimePacing.laps>1) {
            realTimePacing.laps -= 1;
        }
        
        server.send(200, "text/plain", String(realTimePacing.laps, 0));
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




#include "realTimePacing.h"
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

// Function to reset realTimePacing to initial values
void resetRealTimePacing() {
    realTimePacing.status = "Stopped";
    realTimePacing.lap = 5;
    realTimePacing.lapTime = 10.0;
    realTimePacing.is_running = false;
}

void sendRealTimePacingUpdates() {
    // Create a JSON string with the real-time pacing data
    String jsonResponse = "{\"status\": \"" + String(realTimePacing.status) + "\""
                          ", \"lap\": " + String(realTimePacing.lap) + 
                          ", \"lapTime\": " + String(realTimePacing.lapTime, 1) + "}";

    // Send it to all connected WebSocket clients
    webSocket.broadcastTXT(jsonResponse);
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
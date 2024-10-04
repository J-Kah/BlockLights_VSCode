#include "autoPacing.h"
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

void sendAutoPacingUpdates() {
    // Create a JSON string with the real-time pacing data
    String jsonResponse = "{\"autoStatus\": \"" + String(autoPacing.autoStatus) + "\""
                          ", \"autoLaps\": " + String(autoPacing.autoLaps) + 
                          ", \"autoTime\": " + String(autoPacing.autoLaps) + 
                          ", \"autoCountdown\": " + String(autoPacing.autoCountdown) + "}";

    // Send it to all connected WebSocket clients
    webSocket.broadcastTXT(jsonResponse);
}

// Define the initialization function
void initAutoPacingRoutes(WebServer &server) {

    // Serve the Auto Pacing page
    server.on("/autoPacing", []() {
        if(ensureRedirect("/autoPacing")) {
            return;
        }else {
            serveFile("/autoPacing.html", "text/html");
        }
    });

    // Serve auto Pacing JavaScript
    server.on("/autoPacing.js", []() {
        serveFile("/scripts/autoPacing.js", "application/javascript");
    });

    // Routes for fetching auto Pacing values
    server.on("/getAutoLaps", [&]() {
        if(long(autoPacing.autoLaps) % 1 == 0){
            server.send(200, "text/plain", String(autoPacing.autoLaps, 0));
        } else {
            server.send(200, "text/plain", String(autoPacing.autoLaps, 1)); // Send lap value as plain text
        }
    });

    // Route to get total time
    server.on("/getAutoTime", [&]() {
        int mins = autoPacing.autoTime / 60;
        float seconds = autoPacing.autoTime - mins*60;
        server.send(200, "text/plain", String(mins) + ":" + String(seconds, 2)); // Send time with 2 decimal places
    });

    // Route to get running status
    server.on("/getAutoStatus", [&]() {
        server.send(200, "text/plain", autoPacing.autoStatus); // Send running status
    });

    server.on("/getAutoCountdown", [&]() {
        server.send(200, "text/plain", autoPacing.autoCountdown ? "Yes" : "No");
    });

    server.on("/setAutoLaps", [&]() {
        String lapsStr = server.arg("plain"); // Retrieve the incoming time string
        
        // Convert the string to a float and store it
        if(lapsStr.toFloat() >= 0.25) {
            autoPacing.autoLaps = round(2*lapsStr.toFloat())/2.0;
        }

        if(int(2*autoPacing.autoLaps) % 2 == 0){
            server.send(200, "text/plain", String(autoPacing.autoLaps, 0));
        } else {
            server.send(200, "text/plain", String(autoPacing.autoLaps, 1)); // Send lap value as plain text
        }
    });

    server.on("/setAutoTime", [&]() {
        String timeStr = server.arg("plain"); // Retrieve the incoming time string
        // Convert the string to a float and store it
        if(timeStr.toFloat() >= 5 && timeStr.toFloat() < 6000){
            autoPacing.autoTime = timeStr.toFloat();
        }

        // Respond to the client with a success message
        int mins = autoPacing.autoTime / 60;
        float seconds = autoPacing.autoTime - mins*60;
        server.send(200, "text/plain", String(mins) + ":" + String(seconds, 2)); // Send time with 2 decimal places
    });


    server.on("/toggleAutoPacing", [&]() {
        // Turn on or off the running status
        autoPacing.autoIsRunning = !autoPacing.autoIsRunning;
        if(autoPacing.autoIsRunning) {
            startAutoPacingTask();
            autoPacing.autoStatus = "Running";
        } else {
            autoPacing.autoStatus = "Stopped";
        }

        server.send(200, "text/plain", autoPacing.autoStatus);
    });

    server.on("/toggleAutoCountdown", [&]() {
        // Turn on or off the running status
        autoPacing.autoCountdown = !autoPacing.autoCountdown;
        server.send(200, "text/plain", autoPacing.autoCountdown ? "Yes" : "No");
    });

}
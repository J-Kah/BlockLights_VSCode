#include "blockLightsSettings.h"

#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

void sendSettingsUpdates() {
    // Create a JSON string with the real-time pacing data
    String jsonResponse = "{\"trackNumber\": \"" + String(settings.trackNumber) + "\"}";
    // Send it to all connected WebSocket clients
    webSocket.broadcastTXT(jsonResponse);
}

// Define the initialization function
void initSettingsRoutes(WebServer &server) {

    // get routes
    server.on("/getNumberOfTracks", [&]() {
        server.send(200, "text/plain", settings.numberOfTracks ? "7" : "5"); // Send lap time with 1 decimal place
    });

    server.on("/getStartingOn500mSide", [&]() {
        server.send(200, "text/plain", settings.startingOn500mSide ? "Yes" : "No"); // Send lap time with 1 decimal place
    });

    server.on("/getTrackNumber", [&]() {
        server.send(200, "text/plain", String(settings.trackNumber)); // Send lap time with 1 decimal place
    });


    // Serve the settings page
    server.on("/blockLightsSettings", []() {
        if(ensureRedirect("/blockLightsSettings")) {
            return;
        }else {
            serveFile("/blockLightsSettings.html", "text/html");
        }
    });

    // Serve settings JavaScript
    server.on("/blockLightsSettings.js", []() {
        serveFile("/scripts/blockLightsSettings.js", "application/javascript");
    });

    // Routes for updating settings values (no page reloads)
    server.on("/incrementTrackNumber", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.trackNumber < 7 - (2 * !settings.numberOfTracks)) {
            settings.trackNumber += 1;
        }
        server.send(200, "text/plain", String(settings.trackNumber));
    });

     server.on("/decrementTrackNumber", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.trackNumber > 1) {
            settings.trackNumber -= 1;
        }
        server.send(200, "text/plain", String(settings.trackNumber));
    });

    server.on("/toggleNumberOfTracks", [&]() {
        // Turn on or off the running status
        settings.numberOfTracks = !settings.numberOfTracks;
        if(settings.trackNumber > 5) {
            settings.trackNumber = 5;
        }
        sendSettingsUpdates();
        Serial.println("Toggled Tracks");
        server.send(200, "text/plain", settings.numberOfTracks ? "7" : "5" );
    });

    server.on("/toggleStartingOn500mSide", [&]() {
        // Turn on or off the running status
        settings.startingOn500mSide = !settings.startingOn500mSide;
        server.send(200, "text/plain", settings.startingOn500mSide ? "Yes" : "No");
    });
}
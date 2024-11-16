#include "blockLightsSettings.h"

settings_t settings = {
    true,       // number of tracks (true = 7, false = 5)
    1,          // current track nunmber
    false,      // starting on 500m side (true if starting on 500m side, flase if starting on 1000m side)
    2,          // number of leading blocks  
    1,          // number of pacing blocks
    2,          // number of trailing blocks
    false       // show all blocks
};

static void sendSettingsUpdates() {
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

    server.on("/getNumLeadingBlocks", [&]() {
        server.send(200, "text/plain", String(settings.numLeadingBlocks)); // Send lap time with 1 decimal place
    });

    server.on("/getNumPacingBlocks", [&]() {
        server.send(200, "text/plain", String(settings.numPacingBlocks)); // Send lap time with 1 decimal place
    });

    server.on("/getNumTrailingBlocks", [&]() {
        server.send(200, "text/plain", String(settings.numTrailingBlocks)); // Send lap time with 1 decimal place
    });

    server.on("/getShowAllBlocks", [&]() {
        server.send(200, "text/plain", String(settings.showAllBlocks ? "Yes" : "No")); // Send lap time with 1 decimal place
    });

    // Serve the settings page
    server.on("/blockLightsSettings", []() {
        if(ensureRedirect("/blockLightsSettings")) {
            return;
        }else {
            killPacingTasks(realTimePacing, autoPacing);
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

    server.on("/toggleShowAllBlocks", [&]() {
        // Turn on or off the running status
        settings.showAllBlocks = !settings.showAllBlocks;
        if(settings.showAllBlocks){
            addAllBlocks();
        } else {
            // clear all the virtual blocks
            if (blocks.size() > 1) {
                for(int i=1; i < blocks.size(); i++) {
                    if(blocks[i].status == "Virtual") {
                        blocks.erase(blocks.begin() + i);
                        i--; // check the same index again because we just erased the block in the current index
                    }
                }
            }
        }
        sendSettingsUpdates();
        Serial.println("Toggled showAllBlocks");
        server.send(200, "text/plain", settings.showAllBlocks ? "Yes" : "No" );
    });

    server.on("/toggleStartingOn500mSide", [&]() {
        // Turn on or off the running status
        settings.startingOn500mSide = !settings.startingOn500mSide;
        server.send(200, "text/plain", settings.startingOn500mSide ? "Yes" : "No");
    });

    server.on("/incrementNumLeadingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numLeadingBlocks < 5) {
            settings.numLeadingBlocks += 1;
        }
        server.send(200, "text/plain", String(settings.numLeadingBlocks));
    });

    server.on("/decrementNumLeadingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numLeadingBlocks > 1) {
            settings.numLeadingBlocks -= 1;
        }
        server.send(200, "text/plain", String(settings.numLeadingBlocks));
    });

    server.on("/incrementNumPacingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numPacingBlocks < 5) {
            settings.numPacingBlocks += 1;
        }
        server.send(200, "text/plain", String(settings.numPacingBlocks));
    });

    server.on("/decrementNumPacingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numPacingBlocks > 1) {
            settings.numPacingBlocks -= 1;
        }
        server.send(200, "text/plain", String(settings.numPacingBlocks));
    });

    server.on("/incrementNumTrailingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numTrailingBlocks < 5) {
            settings.numTrailingBlocks += 1;
        }
        server.send(200, "text/plain", String(settings.numTrailingBlocks));
    });

    server.on("/decrementNumTrailingBlocks", [&]() {
        // Increment laps (to be handled in JS)
        if(settings.numTrailingBlocks > 1) {
            settings.numTrailingBlocks -= 1;
        }
        server.send(200, "text/plain", String(settings.numTrailingBlocks));
    });



}

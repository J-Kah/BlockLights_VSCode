#include "autoPacing.h"

extern WebSocketsServer webSocket;  // Access the WebSocket server instance
uint64_t prevToggleTime = 0;

autoPacingClass autoPacing = autoPacingClass("Stopped", 90, 9.0, false, true);

String CRGBtoHexString(const CRGB& colour) {
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

void sendAutoPacingUpdates() {

    int mins = autoPacing.totalTime / 60;
    float seconds = autoPacing.totalTime - mins * 60;

    String laps = String(autoPacing.laps, 1); // Default with 1 decimal place
    if (int(2 * autoPacing.laps) % 2 == 0) {
        laps = String(autoPacing.laps, 0); // Update the same laps variable
    }

    String countDown = autoPacing.countdown ? "Yes" : "No";

    // Ensure seconds have leading zeros if less than 10
    String secondsStr = String(seconds, 2);
    if (seconds < 10) {
        secondsStr = "0" + secondsStr; // Add leading zero for seconds < 10
    }

    String json = "{";
    json += "\"autoStatus\": \"" + String(autoPacing.status.c_str()) + "\",";
    json += "\"autoLaps\": \"" + laps + "\",";
    json += "\"autoTime\": \"" + String(mins) + ":" + secondsStr + "\",";
    json += "\"autoCountdown\": \"" + countDown + "\",";
    json += "\"circles\": [";

    for (size_t i = 0; i < blocks.size(); ++i) {
        json += "{";
        json += "\"id\": \"" + String(blocks[i].blockId.c_str()) + "\",";
        json += "\"color\": \"" + CRGBtoHexString(blocks[i].colour) + "\"";
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
void initAutoPacingRoutes(WebServer &server) {

    // Serve the Auto Pacing page
    server.on("/autoPacing", []() {
        if(ensureRedirect("/autoPacing")) {
            return;
        }else {
            killPacingTasks(realTimePacing, autoPacing);
            serveFile("/autoPacing.html", "text/html");
        }
    });

    // Serve auto Pacing JavaScript
    server.on("/autoPacing.js", []() {
        serveFile("/scripts/autoPacing.js", "application/javascript");
    });

    // Routes for fetching auto Pacing values
    server.on("/getAutoLaps", [&]() {
        if(int(2*autoPacing.laps) % 2 == 0){
            server.send(200, "text/plain", String(autoPacing.laps, 0));
        } else {
            server.send(200, "text/plain", String(autoPacing.laps, 1)); // Send lap value as plain text
        }
    });

    // Route to get total time
    server.on("/getAutoTime", [&]() {
        int mins = autoPacing.totalTime / 60;
        float seconds = autoPacing.totalTime - mins*60;
        server.send(200, "text/plain", String(mins) + ":" + String(seconds, 2)); // Send time with 2 decimal places
    });

    // Route to get running status
    server.on("/getAutoStatus", [&]() {
        server.send(200, "text/plain", autoPacing.status.c_str()); // Send running status
    });

    server.on("/getAutoCountdown", [&]() {
        server.send(200, "text/plain", autoPacing.countdown ? "Yes" : "No");
    });

    server.on("/setAutoLaps", [&]() {
        if(!autoPacing.isRunning) {
            String lapsStr = server.arg("plain"); // Retrieve the incoming time string
            
            // Convert the string to a float and store it
            if(lapsStr.toFloat() >= 0.25) {
                autoPacing.laps = round(2*lapsStr.toFloat())/2.0;
            }

            if(int(2*autoPacing.laps) % 2 == 0){
                server.send(200, "text/plain", String(autoPacing.laps, 0));
            } else {
                server.send(200, "text/plain", String(autoPacing.laps, 1)); // Send lap value as plain text
            }
        }
    });

    server.on("/setAutoTime", [&]() {
        if(!autoPacing.isRunning) {
            String timeStr = server.arg("plain"); // Retrieve the incoming time string
            // Convert the string to a float and store it
            if(timeStr.toFloat() >= 5 && timeStr.toFloat() < 6000){
                autoPacing.totalTime = timeStr.toFloat();
            }

            // Respond to the client with a success message
            int mins = autoPacing.totalTime / 60;
            float seconds = autoPacing.totalTime - mins*60;
            String secondsStr;

            if(seconds < 10){
                secondsStr = "0" + String(seconds, 2);
            } else {
                secondsStr = String(seconds, 2);
            }
            server.send(200, "text/plain", String(mins) + ":" + secondsStr); // Send time with 2 decimal places
        }
    });


    server.on("/toggleAutoPacing", [&]() {
        // Turn on or off the running status

        // limit how quickly we can toggle this
        if(esp_timer_get_time() - prevToggleTime > 500000){
            autoPacing.isRunning = !autoPacing.isRunning;

            if(autoPacing.isRunning) {
                startAutoPacingTask();
                autoPacing.status = "Running";
            } else {
                autoPacing.status = "Stopped";
            }

            prevToggleTime = esp_timer_get_time();
        }

        server.send(200, "text/plain", autoPacing.status.c_str());
    });

    server.on("/toggleAutoCountdown", [&]() {
        if(!autoPacing.isRunning) {
            // Turn on or off the auto countdown
            if(!autoPacing.isRunning){
                autoPacing.countdown = !autoPacing.countdown;
            }
            
            server.send(200, "text/plain", autoPacing.countdown ? "Yes" : "No");
        }
    });

    // Handle requests for background.jpg
    server.on("/track-background.jpg", HTTP_GET, [&]() {
        serveFile("/track-background.jpg", "image/jpeg");
    });

}
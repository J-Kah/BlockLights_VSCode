#include "realTimePacing.h"

realTimePacingClass realTimePacing = realTimePacingClass("Stopped", 5, false, 10.0f);

// Function to reset realTimePacing to initial values
static void resetRealTimePacing() {
    realTimePacing.status = "Stopped";
    realTimePacing.laps = 5;
    realTimePacing.isRunning = false;
    realTimePacing.lapTime = 10.0f;
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
        json += "\"color\": \"" + CRGBtoHexString(blocks[i].colour) + "\"";  // color is a string
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
            realTimePacing.startRealTimePacingTask(autoPacing);
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


void realTimePacingTask(void* pvParameters) {

    float blueOffset, greenOffset, redOffset, offOffset, trackOffset;
    getOffsets(blueOffset, greenOffset, redOffset, offOffset, trackOffset);
    
    float originalNumLaps = realTimePacing.laps;
    float prevDist = 0.0f;
    
    int64_t pacingStartTime = esp_timer_get_time();
    int64_t prevTime = pacingStartTime;
    bool blocksChanged = false;
    while(realTimePacing.isRunning && realTimePacing.laps > 0) {
        int64_t currentTime = esp_timer_get_time();
        int64_t timeDelta = currentTime - prevTime;
        float currentSpeed = trackLength / realTimePacing.lapTime;
        float currentDist = prevDist + (timeDelta/(1000000.0)) * currentSpeed;
        

        // update the lap value if we do a lap
        if(currentDist > trackLength) {
            realTimePacing.laps -= 1;
            currentDist -= trackLength;

            Serial.print("Lap count updated, laps to go: ");
            Serial.println((int)realTimePacing.laps);
            Serial.print("Lapt time: ");
            Serial.println(realTimePacing.lapTime);
            Serial.print("Time delta (us): ");
            Serial.print(timeDelta);
        }
        
        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {
            
            float blockPosition = blockDistances[blocks[i].number - 1] + trackOffset;

            // edge case for crossing the start/finish lines and track lenght being reset
            if(blockPosition - currentDist > trackLength/2) {
                blockPosition -= trackLength;
            } else if(currentDist - blockPosition > trackLength/2) {
                blockPosition += trackLength;
            }

            if(blockPosition < (currentDist + offOffset)) {
                updateBlock(i, CRGB::Black, &blocksChanged);

            } else if(blockPosition < currentDist + redOffset) {
                updateBlock(i, CRGB::Red, &blocksChanged);
                
            } else if(blockPosition < currentDist + greenOffset) {
                updateBlock(i, CRGB::Green, &blocksChanged);
                
            } else if(blockPosition < currentDist + blueOffset) {
                updateBlock(i, CRGB::Blue, &blocksChanged);
            }
        }

        if(blocksChanged) {
            sendRealTimePacingUpdates();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        blocksChanged = false;
        prevDist = currentDist;
        prevTime = currentTime;

    }

    Serial.println("Pacing finished (or stopped)");

    // Turn all blocks off
    for (int i = 0; i < size(blocks); i++) {
        blocks[i].colour = CRGB::Black;
    }

    updateLEDs();

    // reset the lap count for the next set
    realTimePacing.laps = originalNumLaps;
    // send updates to client?
    realTimePacing.isRunning = false;
    realTimePacing.status = "Stopped";
    sendRealTimePacingUpdates();

    
    vTaskDelay(50 / portTICK_PERIOD_MS); // Wait a little bit before deleting the task
    vTaskDelete(NULL);  // Delete this task
}

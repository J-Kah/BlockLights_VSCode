#include "autoPacing.h"

static uint64_t prevToggleTime = 0;

autoPacingClass autoPacing = autoPacingClass("Stopped", 90, 9.0, false, true);

// speed profile for auto pacing. Numbers in percentage of top speed (probably needs to be tuned)
static const float autoPacingDefaultSpeedProfile[9] = {
    0.3241, // Speed at time 0. Starting is very punchy, not good to start at 0% speed.
    0.9000, // Speed at 0.5 laps
    0.9677, // speed at 1 lap
    1.0000, // speed at 1.5 laps

    1.0000, // speed at 2 laps from finish
    0.9890, // speed at 1.5 laps from finish
    0.9783, // speed at 1 lap from finish
    0.9677, // speed at 0.5 laps from finish
    0.9574  // speed at finish
};

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
            serveFile("/htmls/autoPacing.html", "text/html");
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
                autoPacing.startAutoPacingTask(realTimePacing);
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

void autoPacingTask(void* pvParameters) {
    float blueOffset, greenOffset, redOffset, offOffset, trackOffset;
    getOffsets(blueOffset, greenOffset, redOffset, offOffset, trackOffset);


    std::vector<float> autoPacingSpeedProfile;
    // Need to get array of speed percentages, per half lap
    for(int i = 0; i <= 2*autoPacing.laps; i++){
        if(i <= 8) { // 9 is the size of autoPacingDefaultSpeedProfile
             autoPacingSpeedProfile.push_back(autoPacingDefaultSpeedProfile[i]);
        } else{
            autoPacingSpeedProfile.insert(autoPacingSpeedProfile.begin() + 4, 1.00); // insert half laps at 1.00 speed after 1.5 laps
        }
       
    }

    // calculate the 100% speed for given time (linear acceleration assumed)
    float temp = 0.0;
    for(int i = 0; i < size(autoPacingSpeedProfile)-1; i++) {
        temp += 1/(autoPacingSpeedProfile[i+1] + autoPacingSpeedProfile[i]);
    }
    float maxSpeed = trackLength*temp/autoPacing.totalTime; // max speed in terms of m/s
    Serial.println("Max speed:");
    Serial.println(maxSpeed);

    // convert percentage max speed values into m/s
    for (int i = 0; i < size(autoPacingSpeedProfile); ++i) {
        autoPacingSpeedProfile[i] = maxSpeed*autoPacingSpeedProfile[i];
    }


    // 5 second countdown
    if(autoPacing.countdown){
        countdown();        
    }

    // check if pacing was stopped during countdown (?)
    if(!autoPacing.isRunning){
        Serial.println("Pacing finished (or stopped)");

        // Turn all blocks off
        updateAllBlocks(CRGB::Black);
        autoPacing.status = std::string("Stopped");
        sendAutoPacingUpdates();
        vTaskDelay(50 / portTICK_PERIOD_MS); // let other tasks run, needed to finish sending autopacing updates
        vTaskDelete(NULL);  // Delete this task
    }
    

    // calculate the total distance:
    float totalDist = autoPacing.laps * trackLength;
    int originalNumLaps = floor(autoPacing.laps);

    // Start AutoPacing!
    int numHalfLaps = 0;
    float currentDist = 0.0;
    float prevLaptime = 0.0;

    Serial.println("Started AutoPacing");
    int64_t pacingStartTime = esp_timer_get_time();
    int64_t prevHalfLapTime = pacingStartTime;

    if(autoPacing.countdown){   // wait for 1 second of green
        while(esp_timer_get_time() - pacingStartTime < 1000000){
            vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        }

        // Turn all blocks off
        updateAllBlocks(CRGB::Black);
    }


    bool blocksChanged = false;
    while(autoPacing.isRunning && currentDist < totalDist) {
        int64_t currentTime = esp_timer_get_time();

        float sPrev = autoPacingSpeedProfile[numHalfLaps]; // m/s speed of the previous half lap checkpoint
        float sNext = autoPacingSpeedProfile[numHalfLaps+1]; // m/s speed of the next half lap checkpoint

        float currentSpeed = (sNext*sNext - sPrev*sPrev)*(currentTime - prevHalfLapTime)/(trackLength*1000000) + sPrev; 

        float currentLaptime = trackLength/currentSpeed;

        if(fabs(currentLaptime - prevLaptime) > 0.01) {
            // update the status to show the current speed
            String truncatedLaptime = String(currentLaptime, 2);
            autoPacing.status = "Running. Laptime: " + std::string(truncatedLaptime.c_str());
            prevLaptime = currentLaptime;

            // update the webserver 
            sendAutoPacingUpdates(); 
        }

        currentDist = numHalfLaps*trackLength/2 + (currentTime - prevHalfLapTime)*(currentSpeed + sPrev)/2000000;
        
        // update previous half lap time. i is the number of half laps (NEEDS to be after line above, otherwise ends in a loop because prevHalfLaptTime is calculated after currentTime.)
        if(2*currentDist > (numHalfLaps+1)*trackLength){
            numHalfLaps++;
            prevHalfLapTime = currentTime;
        }

        // update lap count
        if(int((2*autoPacing.laps))%2 == 1 && 2*currentDist > trackLength ){
            autoPacing.laps -= 0.5;
            Serial.print("Lap count updated, laps to go: ");
            Serial.println(autoPacing.laps);
            sendAutoPacingUpdates();
        } else if(currentDist > (originalNumLaps - autoPacing.laps + 1)*trackLength + trackLength*settings.startingOn500mSide/2){
            Serial.print("Total distance: ");
            Serial.println(currentDist);
            autoPacing.laps -= 1;
            Serial.print("Lap count updated, laps to go: ");
            Serial.println(autoPacing.laps);
            sendAutoPacingUpdates();
        }

        // calculate current distance around the track (start/finish line is exactly 0m)
        float lapDist = currentDist + trackLength*settings.startingOn500mSide/2;
        while(lapDist > trackLength){
            lapDist -= trackLength;
        }

        // change blocks based on distance
        for (int i = 0; i < size(blocks); i++) {

            float blockPosition = blockDistances[blocks[i].number - 1] + trackOffset;

            // edge case for crossing the start/finish lines and track length being reset
            if(blockPosition - lapDist > trackLength/2) {
                blockPosition -= trackLength;
            } else if(lapDist - blockPosition > trackLength/2) {
                blockPosition += trackLength;
            }

            if(blockPosition < (lapDist + offOffset)) {
                updateBlock(i, CRGB::Black, &blocksChanged);

            } else if(blockPosition < lapDist + redOffset) {
                updateBlock(i, CRGB::Red, &blocksChanged);

            } else if(blockPosition < lapDist + greenOffset) {
                updateBlock(i, CRGB::Green, &blocksChanged);

            } else if(blockPosition < lapDist + blueOffset) {
                updateBlock(i, CRGB::Blue, &blocksChanged);
            }
        }

        if(blocksChanged) {
            sendAutoPacingUpdates();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // let other tasks run
        blocksChanged = false;
    }
    Serial.print("TOTAL TIME TAKEN: ");
    Serial.println(String((esp_timer_get_time() - pacingStartTime)/1000000.0, 3));
    Serial.println(autoPacing.totalTime);

    Serial.println("Pacing finished (or stopped)");

    // Turn all blocks off
    updateAllBlocks(CRGB::Black);

    // send updates to client
    autoPacing.laps = originalNumLaps + 0.5*settings.startingOn500mSide;
    autoPacing.isRunning = false;
    autoPacing.status = "Stopped";
    sendAutoPacingUpdates();

    vTaskDelay(50 / portTICK_PERIOD_MS); // let other tasks run, needed to finish sending autopacing updates
    
    vTaskDelete(NULL);  // Delete this task
}
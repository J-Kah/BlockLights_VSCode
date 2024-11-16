#include "blockLightsSetup.h"

void sendSetupUpdates() {

    int numBlocks = blocks.size();

    // Order blocks in order of block number
    for(int i=0; i < numBlocks - 1; i++) {
        for(int j=0; j < numBlocks - 1; j++) {
            if(blocks[j+1].number < blocks[j].number){
                std::swap(blocks[j], blocks[j+1]);
            }
        }
    }

    writeBlocksToFile();

    // Create a JSON string with the block data
    String jsonResponse = "[";

    // Manually create the JSON string
    for (int i = 0; i < numBlocks; i++) {
        jsonResponse += "{";
        
        // Convert MAC address to string format
        jsonResponse += "\"MACAddress\":\"";
        for (int j = 0; j < 6; j++) {
            if (j > 0) jsonResponse += ":";
            String hexPart = String(blocks[i].mac[j], HEX);
            hexPart.toUpperCase();  // Convert to uppercase
            if (hexPart.length() < 2) jsonResponse += '0';  // Add leading zero if necessary
            jsonResponse += hexPart;
        }
        jsonResponse += "\",";
        
        // Add number and status fields
        jsonResponse += "\"number\":" + String(blocks[i].number) + ",";
        jsonResponse += "\"status\":\"" + String(blocks[i].status.c_str()) + "\"";
        
        jsonResponse += "}";
        if (i < numBlocks - 1) {
            jsonResponse += ",";
        }
    }
    jsonResponse += "]";


    // Send it to all connected WebSocket clients
    webSocket.broadcastTXT(jsonResponse);
}

// Utility function to convert uint8_t MAC to String
static std::string formatMACAddress(const uint8_t (&mac)[6]) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

static int extractNumber(const String& label, const String& body) {
    int startIdx = body.indexOf(label + "\":\"") + label.length() + 3;
    int endIdx = body.indexOf("\"", startIdx);
    return body.substring(startIdx, endIdx).toInt();
}

void initSetupRoutes(WebServer &server) {

    server.on("/updateBlockNumber", [&]() {
        if(!blinking) {
            String body = server.arg("plain");

            int numFrom = extractNumber("numFrom", body);
            int numTo = extractNumber("numTo", body);

            int idxFrom = -1;
            int idxTemp = -1;

            // find index(s) of blocks to change
            for(int i=0; i<blocks.size(); i++) {
                if(blocks[i].number == numTo){
                    idxTemp = i;
                }
                if(blocks[i].number == numFrom){
                    idxFrom = i;
                }
            }

            // Make sure the block is (or was recently) connected
            if(blocks[idxFrom].status == "Disconnected") {
                return;
            }

            // change idxFrom to numTo
            blocks[idxFrom].number = numTo;
            blocks[idxFrom].blockId = "block" + std::to_string(numTo);
            // update slave block number
            if(blocks[idxFrom].status != "Virtual") {
                updateBlockNumber(idxFrom, numTo);
            } else {
                blocks[idxFrom].mac[5] = (uint8_t)numTo;
            }

            // change numTo (if previously found) to numFrom
            if(idxTemp != -1){
                blocks[idxTemp].number = numFrom;
                blocks[idxTemp].blockId = "block" + std::to_string(numFrom);
                if(blocks[idxTemp].status != "Virtual") {
                    updateBlockNumber(idxTemp, numFrom);
                } else {
                    blocks[idxTemp].mac[5] = (uint8_t)numFrom;
                }
            }
        }
        // Might need a delay here if blocks are getting sent updates after being sorted -> wrong index

        // send updates
        sendSetupUpdates();

    });

    server.on("/clearBlock", [&]() {
        if(!blinking){
            std::string macAddress = server.arg("plain").c_str();

            // Loop through the array to find the block with matching MAC address
            for (size_t i = 0; i < blocks.size(); i++) {
                std::string blockMac = formatMACAddress(blocks[i].mac); // Convert MAC to String for comparison

                if (blockMac == macAddress) {
                    // Erase the struct from the array
                    removePeer(blocks[i].mac);
                    blocks.erase(blocks.begin() + i);
                    break;
                }
            }
        }

        sendSetupUpdates();

        server.send(200); // 
    });

    server.on("/blinkBlock", [&]() {
        std::string macAddress = server.arg("plain").c_str();

        if(!blinking) {
            blinking = 1;

            // Loop through the array to find the block with matching MAC address
            for (size_t i = 0; i < blocks.size(); i++) {
                std::string blockMac = formatMACAddress(blocks[i].mac); // Convert MAC to String for comparison

                if (blockMac == macAddress) {
                    blinkBlock(blockMac, i);
                    break;
                }
            }
        }
        server.send(200); // 
    });

    server.on("/blinkAll", [&]() {
        if(!blinking) {
            blinking = 1;
            blinkAll();
        }
        // scan for blocks

        server.send(200); // 
    });

    server.on("/scanBlocks", [&]() {

        if(!blinking) {
            // Scan for any new blocks and add them
            scanForBlocks();
        }

        server.send(200); // 
    });

    server.on("/autoBlockSetup", [&]() {

        // determin block position, maybe

        server.send(200); // 
    });

    server.on("/clearAllBlocks", [&]() {

        if(!blinking){
            // clear all the blocks
            if (blocks.size() > 1) {

                for(int i=1; i < blocks.size(); i++) {
                    removePeer(blocks[i].mac);
                }


                blocks.erase(blocks.begin() + 1, blocks.end());
            }
        }

        sendSetupUpdates();
        server.send(200); // 
    });

    // serve the html
    server.on("/blockLightsSetup", []() {
        if(ensureRedirect("/blockLightsSetup")) {
            return;
        }else {
            killPacingTasks(realTimePacing, autoPacing);
            serveFile("/blockLightsSetup.html", "text/html");
        }
    });

    // Serve setup JavaScript
    server.on("/blockLightsSetup.js", []() {
        serveFile("/scripts/blockLightsSetup.js", "application/javascript");
    });



    server.on("/getBlockData", [&]() {    
        int numBlocks = blocks.size();

        // Order blocks in order of block number
        for(int i=0; i < numBlocks - 1; i++) {
            for(int j=0; j < numBlocks - 1; j++) {
                if(blocks[j+1].number < blocks[j].number){
                    std::swap(blocks[j], blocks[j+1]);
                }
            }
        }

        // Create a JSON string with the block data
        String jsonResponse = "[";

        // Manually create the JSON string
        for (int i = 0; i < numBlocks; i++) {
            jsonResponse += "{";
            
            // Convert MAC address to string format
            jsonResponse += "\"MACAddress\":\"";
            for (int j = 0; j < 6; j++) {
                if (j > 0) jsonResponse += ":";
                String hexPart = String(blocks[i].mac[j], HEX);
                hexPart.toUpperCase();  // Convert to uppercase
                if (hexPart.length() < 2) jsonResponse += '0';  // Add leading zero if necessary
                jsonResponse += hexPart;
            }
            jsonResponse += "\",";
            
            // Add number and status fields
            jsonResponse += "\"number\":" + String(blocks[i].number) + ",";
            jsonResponse += "\"status\":\"" + String(blocks[i].status.c_str()) + "\"";
            
            jsonResponse += "}";
            if (i < numBlocks - 1) {
                jsonResponse += ",";
            }
        }
        jsonResponse += "]";


        server.send(200, "text/plain", jsonResponse); // Send lap time with 1 decimal place
    });

}
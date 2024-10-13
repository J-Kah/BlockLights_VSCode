#include "blockLightsSetup.h"
#include <WebSocketsServer.h>
#include <FastLED.h>
#include <algorithm>

extern WebSocketsServer webSocket;  // Access the WebSocket server instance

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
        jsonResponse += "\"status\":\"" + blocks[i].status + "\"";
        
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
String formatMACAddress(uint8_t mac[6]) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

int extractNumber(String label, String body) {
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

            // change idxFrom to numTo
            blocks[idxFrom].number = numTo;
            blocks[idxFrom].blockId = "block" + String(numTo);
            // update slave block number
            updateBlockNumber(idxFrom, numTo);

            // change numTo (if previously found) to numFrom
            if(idxTemp != -1){
                blocks[idxTemp].number = numFrom;
                blocks[idxTemp].blockId = "block" + String(numFrom);
                updateBlockNumber(idxTemp, numFrom);
            }
        }
        // Might need a delay here if blocks are getting sent updates after being sorted -> wrong index

        // send updates
        sendSetupUpdates();

    });

    server.on("/clearBlock", [&]() {
        if(!blinking){
            String macAddress = server.arg("plain");

            // Loop through the array to find the block with matching MAC address
            for (size_t i = 0; i < blocks.size(); i++) {
                String blockMac = formatMACAddress(blocks[i].mac); // Convert MAC to String for comparison

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
        String macAddress = server.arg("plain");

        if(!blinking) {
            blinking = 1;

            // Loop through the array to find the block with matching MAC address
            for (size_t i = 0; i < blocks.size(); i++) {
                String blockMac = formatMACAddress(blocks[i].mac); // Convert MAC to String for comparison

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
            killPacingTasks();
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
            jsonResponse += "\"status\":\"" + blocks[i].status + "\"";
            
            jsonResponse += "}";
            if (i < numBlocks - 1) {
                jsonResponse += ",";
            }
        }
        jsonResponse += "]";


        server.send(200, "text/plain", jsonResponse); // Send lap time with 1 decimal place
    });

    


}
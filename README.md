<pre>
| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ?---- | ?------- | ?------- | ?--------| YES----- | ?------- | ?------- | ?------- | ?------- |
</pre>
# BlockLights Master

This is the alpha version of BlockLights Master code, written in VSCode with the ESP-IDF. For the slave repository for ESP32s, see the [GitHub](https://github.com/J-Kah/BlockLights_Slave).

This is a pacing project for the sport of short track speed skating. There should be 1 master ESP (preferable ESP32) and 13 slave ESPs (ESP8266 or ESP32). The ESPs are to be placed inside the track markers and will control LEDs to set the pace for the skaters. The Master ESP hosts a webserver, which can be connected to by any device over WiFi. Just connect to "BlockLights" Wifi and go to blocklights.local in any web browser.

Currently there are two pacing modes implemented -
1. Real-Time Pacing: Set a lap time and number of laps, and the blocklights will pace at that speed. Can be adjusted on-the-fly.<br>
2. Autopacing: Set a distance and a total time (e.g. for a time trial, standing start), and the blocklights will calculate acceleration periods for the time trial so that the pacing finishes exactly at the time specified.

## How to setup this code in VSCode
1. Follow the instructions [here](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md) to set up VSCode and the ESP-IDF environment
2. To setup the project folder:
    1. In VSCode press ctrl + P to open the quick open feature
    2. enter ">ESP-IDF: Welcome"
    3. Make sure the ESP32 is plugged in to your laptop so we can select the proper COM port later
    4. Create new project
    5. Set board to your ESP32 (should be ESP32-C6), set the COM port to whichever your ESP32 is connected to
    6. Create Project (wait for it to complete, then open the new project folder either by clicking the popup or file -> open folder...)
3. Set the ESP IDF target (this should)
    1. Ctrl + p in VSCode again
    2. Enter ">ESP-IDF: Set Espressif Device Target"
    3. Select your ESP32 board again, and then the Jtag option
4. Open the ESP-IDF terminal (Ctrl + p, enter ">ESP-IDF: Open ESP-IDF Terminal")
5. Add arduino and LittleFS dependencies into the ESP-IDF
    1. In the ESP-IDF terminal, enter "idf.py add-dependency "espressif/arduino-esp32^3.1.0-rc1"
    2. In the ESP-IDF terminal, enter "idf.py add-dependency "joltwallet/littlefs^1.14.8"
6. Configure sdkconfig file for ESP32
    1. Press ctrl + shift + E to open the project folder explorer
    2. Open sdkconfig file near the bottom
    3. search for "CONFIG_FREERTOS_HZ" and change it from CONFIG_FREERTOS_HZ=100 to CONFIG_FREERTOS_HZ=1000.
    4. Press ctrl + S to save
7. Change Compiler option in menuconfig
    1. In the ESP-IDF terminal, enter "idf.py menuconfig"
    2. Using the arrow keys, navigate to and change: menuconfig -> Serial flasher config -> Flash size -> Change to however much flash your ESP32 has (E.g.Beetle ESP32 C6 Mini has 4MB)
    3. Using the arrow keys, navigate to and change: menuconfig -> Partition Table -> Partition Table -> Change to Custom partition table CSV
    4. Using the arrow keys, navigate to and change: menuconfig -> Component config -> ESP System Settings -> Channel for consol output, change to USB Serial/JTAG Controller
8. Copy project files 
    1. Download the zip from [this GitHub repository](https://github.com/J-Kah/BlockLights_Slave) (green code button -> Download ZIP)
    2. Extract the folder somewhere, and copy the following files/ folders into your VSCode project root folder (overwrite if necessary):
        1. /components/ folder and all its contents
        2. /main/ folder and all its contents
        3. /managed_components/ folder and all its contents
        4. /partition/ folder and all its contents
        5. CMakeLists.txt
        6. partitions.csv
    3. Delete the file /main/main.c
9. Edit the NUM_LEDS and DATA_PIN values in /main/main.cpp to suit your project
10. In main.cpp, double check "FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);" includes your LED type and colour order (GRB or RGB)
11. In the ESP-IDF terminal, enter "idf.py flash" (it will take a long time to build for the first time, then it will flash to the ESP32)
12. Once flashed, you can monitor the serial by going to the Serial monitor and selecting your COM port & Baud rate -> Start Monitoring.

## Known issues
1. FastLED does not work in DMA mode with the ESP32 C6. It resorts to bit-banging the GPIO pin to send data, which triggers ESP info logs that pollute the serial monitor. It still works but it's annoying.
2. Setting the GPIO Log level to anything but INFO messes with the data being bit-banged to the LEDs which results in incorrect colours being displayed, so that's not viable.

## Potential Issues
1. WebServer inside a freeRTOS task works pretty well, but can get overloaded - I think I've run into crashes / slowdowns because of it but I can't pinpoint exactly when/where in the code it happens.
2. ESP-NOW sned/receive to 13 different blocks at once - untested, may slow down pacing code.

## Future improvements
1. Use AsyncWebServer instead of WebServer for better async handling
2. Use ESP_WebSocket_client as a more native library to the ESP-IDF instead of using the WebSockets which is wrapped in the Arduino interpreter
3. Same for mDNS / DNSServer?
4. Once the GPIO issue is resolved: lower global debugging level, change compilation options to be optimised for performance, change Serial.print to ESP_LOGs & remove USB CDC settings, change Serial output back to deafult
5. Improve autopacing interpolation (Acceleration is currently linear, it probably should be cubic. Make it customisable through the web app?)
6. Add a 3rd pacing mode "Advanced Pacing" where you can choose your lap speed for each lap/half lap. Also can choose a 0/1/2 lap wind-in & countdown
7. Tidy everything up, add descriptions in the web app, more incremental buttons (e.g. change lap times by +/-0.1s or +/- 0.5s)
8. Implement and display a code version in the web app
9. Maybe implement error handling for ESP-NOW between Master and Slave

## Folder contents

The project **BlockLights Master** contains one source file in CPP language [main.cpp](main/main.cpp). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── components
│   └── FastLED                 This is the modified FastLED library
├── CMakeLists.txt              This is the build configuration for the whole project
├── main
│   ├── CMakeLists.txt          This is the build configuration for the /main/ folder.  Command to flash /partitions/ folder with the code inside
│   ├── main.cpp                This is the main code
│   └── ...                     Other custom libraries are in this folder
├── partition           
│   ├── scripts                 This folder contains the javascripts for the websever
│   ├── styles                  This folder contains the styles for the websever       
│   ├── blocks.txt              This is the file accessed by main.cpp to save (read & write) the list of blocks
│   └── ...                     Other html files for the webserver, as well as the ice rink background and favicon
├── partitions.csv              This is the csv file that tells the compiler how to partition the ESP32's flash memory
└── README.md                   This is the file you are currently reading
```

## List of manual changes to source code (Not needed if you downloaded the project files from this repository):
1. FastLED
    1. Change the FastLED folder name inside /components/ from "FastLED-master" to just "FastLED"
    2. In /components/FastLED/CMakeLists.txt, change line 16 from "REQUIRES arduino)" to "REQUIRES arduino-esp32)"
    3. In /components/FastLED/src/platforms/esp/32/led_strip/rmt_strip.cpp, change line 19 from #define TAG "rtm_strip.cpp" to #define TAG "rmt_strip.cpp"
    4. In /components/FastLED/src/platforms/esp/32/led_strip/rmt_strip.cpp, replace the code below line 151 with:
    <pre>
    private:
        led_strip_handle_t mLedStrip = nullptr;
        uint8_t* mBuffer = nullptr;
        bool mAquired = false;
        uint16_t mT0H = 0;
        uint16_t mT0L = 0;
        uint16_t mT1H = 0;
        uint16_t mT1L = 0;
        uint32_t mTRESET = 0;
        bool mIsRgbw = false;
        int mPin = -1;
        uint32_t mMaxLeds = 0;
    };</pre>
    

    5. In /components/FastLED/src/platforms/esp/32/led_strip/defs.h, change line 30 from "#define FASTLED_RMT_WITH_DMA 1" to "#define FASTLED_RMT_WITH_DMA 0"
2. Custom stream and parsing libraries: 
    1. In \managed_components\espressif__arduino-esp32\cores\esp32\Stream.h, add the following code below line 109:
        <pre>String readHTTPUntil(char terminator);</pre>
    2. In \managed_components\espressif__arduino-esp32\cores\esp32\Stream.cpp, add the following code below line 318:
    <pre>
    String Stream::readHTTPUntil(char terminator) {
    String ret;
    int c = timedRead();
    if(c == 'G' || c == 'P' || c == 'H' || c == 'D' || c == 'O' || c == 'C' || c == 'T') { // Check first char for valid HTTP request
        while (c >= 0 && c != terminator) {
        ret += (char)c;
        c = timedRead();
        }
    } else {
        Serial.println("REQUEST WAS NOT A COMMON HTTP REQUEST");
        ret = "badRequest";
    }
    return ret;
    }</pre>
    3. In \managed_components\espressif__arduino-esp32\libraries\WebServer\src\Parsing.cpp, change line 78 from "String req = client.readStringUntil('\r');" to "String req = client.readHTTPUntil('\r');"
    4. In \managed_components\espressif__arduino-esp32\libraries\WebServer\src\Parsing.cpp, add below line 78 the following:
    <pre>
    if(req == "badRequest"){
        return 0;
    }</pre>
3. Custom WebServer.cpp to allow better debugging for unhandles URI requests
    1.  Change \managed_components\espressif__arduino-esp32\libraries\WebServer\src\WebServer.cpp, code at line 793 with:
    <pre>
    // Ignore client HTTP requests for a captive portal
    if(!(_currentUri == "/generate_204" || _currentUri == "/gen_204" || _currentUri == "/chat" || _currentUri == "/connecttest.txt" || _currentUri == "/redirect")) {
        Serial.println(_currentUri);
        log_e("request handler not found");
    }</pre>
4. Added WebSockets.h into the Arduino ESP folder
    1. Download the zip file from the [here](https://github.com/Links2004/arduinoWebSockets)
    2. Extract and place into \managed_components\espressif__arduino-esp32\libraries
    3. Edit \managed_components\espressif__arduino-esp32\CMakeLists.txt to include "WebSockets" in the set(ARDUINO_ALL_LIBRARIES ... ) function
    4. Edit that same CMakeLists.txt to include the following set function:
    <pre>
    set(ARDUINO_LIBRARY_WebSockets_SRCS
        libraries/WebSockets/src/SocketIOclient.cpp
        libraries/WebSockets/src/WebSockets.cpp
        libraries/WebSockets/src/WebSocketsClient.cpp
        libraries/WebSockets/src/WebSocketsServer.cpp)</pre>
5. Edited root project CMakeLists.txt for Serial ouput: Add the below code above the include function
    <pre>
    add_definitions(
        -DARDUINO_USB_MODE
        -DARDUINO_USB_CDC_ON_BOOT
    )</pre>


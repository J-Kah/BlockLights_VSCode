<pre>
| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ?---- | ?------- | ?------- | ?--------| YES----- | ?------- | ?------- | ?------- | ?------- |
</pre>
# BlockLights Slave

This is the alpha version of BlockLights Slave code, written in VSCode with the ESP-IDF. For a better overview of the project, see the [Master repository on GitHub](https://github.com/J-Kah/BlockLights_VSCode).


## How to use this repository
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
    4. Using the arrow keys, navigate to and change: menuconfig -> Component config -> Wi-Fi -> WiFi NVS flash (Change it to unchecked)
8. Add and configure FastLED to components in the project
    1. In the VSCode project directory, create a folder named "components"
    2. Download FastLED from [here](https://github.com/FastLED/FastLED) by clicking the green code button, then Download ZIP
    3. Extract the FastLED folder and move it into the newly created components folder
    4. Change the FastLED source code to work with ESP-IDF (Don't froget to press ctrl + S to save before closing the files)
9. Copy project files 
    1. Download the zip from [this GitHub repository](https://github.com/J-Kah/BlockLights_Slave) (green code button -> Download ZIP)
    2. Extract the folder somewhere, and copy the following files/ folders into your VSCode project root folder (overwrite if necessary):
        1. /components/ folder and all its contents
        2. /main/ folder and all its contents
        3. /partition/ folder and all its contents
        4. partitions.csv
    3. Change partitions.csv to suit your ESP's flash storage (E.g. Beetle ESP32 C6 Mini has 4MB)
        1. Change the last line to "storage,  data, spiffs,  0x310000, 512K,"
    4. Delete the file /main/main.c
10. In the ESP-IDF terminal, enter "idf.py flash" (it will take a long time to build for the first time, then it will flash to the ESP32)

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

## Example folder contents

The project **BlockLights Slave** contains one source file in CPP language [main.cpp](main/main.cpp). The file is located in folder [main](main).

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
│   └── main.cpp                This is the main code
├── partition                   
│   └── blockNumber.txt         This is the file accessed by main.cpp to save (read & write) the block number
├── partitions.csv              This is the csv file that tells the compiler how to partition the ESP32's flash memory
└── README.md                   This is the file you are currently reading
```

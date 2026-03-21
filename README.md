# 30.117 Wireless Comm. & IoT Project

## Introduction

This IoT Project is a humidity and temperature sensor, primarily for use in camera dryboxes. It may also be used for any closed area that needs its humidity monitored, such as 3D printer filament dryers. The project consists of an ESP32C6 DevkitC-1 as its main microcontroller, a SHT31 Humidity and Temperature sensor, and a 3D printed casing to hold the circuitry and a portable power source. ESP Rainmaker is used as the main cloud service for this IoT project. 

## Directories

* `.devcontainer`
* `.vscode`
* `CAD_Files` - Files containing the Fusion 360 model of the holder
* `Docs` - Files containing the dimensions, circuit diagram and user guides of the project
* `main` - Main folder containing C file for firmware, CMakeList, and configuration file for the menu
    * `/CMakeLists.txt` - CMake file for main folder
    * `/idf_component.yml` - YML file detailing required dependencies for this project
    * `/Kconfig.projbuild` - Custom configuration file for easy troubleshooting
    * `/main.c` - Main C file containing firmware code for the ESP32C6
* `.clangd`
* `CMakeLists.txt` - CMake file for overall project
* `dependencies.lock`
* `partitions_4mb_optmimised.csv` - CSV file to detail how the ESP32C6 flash memory will be partitioned
* `sdkconfig.defaults` - Configuration file for project
* `sdkconfig.defaults.esp32` - Configuration file specific to ESP32 boards
* `sdkconfig.defaults.esp32c6` - Configuration file specific to ESP32C6 boards

## Dependencies

The following ESP-IDF components are required for this project:

* [ESP RainMaker Agent Component](https://components.espressif.com/components/espressif/esp_rainmaker/versions/1.12.1/readme)
* [RainMaker App Network Component](https://components.espressif.com/components/espressif/rmaker_app_network/versions/1.4.0/readme)
* [RainMaker App Insights Component](https://components.espressif.com/components/espressif/rmaker_app_insights/versions/1.0.0/readme)
* [esp-idf-lib/sht3x](https://components.espressif.com/components/esp-idf-lib/sht3x/versions/1.0.8/readme)

## User Guide

* [Getting Started](Docs/GettingStarted.md)
* [Configuring](Docs/Configuring.md)
* [Hardware](Docs/Hardware.md)
* [Normal Usage](Docs/NormalUsage.md)

## Known issues

The ESP Rainmaker App on IOS does not have a UI type for one of the components. To fix this, go to `main/main.c`, line 218, and change `ESP_RMAKER_UI_TRIGGER` to `ESP_RMAKER_UI_TOGGLE`. 

# Top-level CMakeLists.txt for AirMaster Adapter ESP32-S3

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set project name
project(airmaster-adapter-esp32)

# SPIFFS image generation using spiffsgen.py
set(SPIFFS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spiffs")
set(SPIFFS_BIN "${CMAKE_BINARY_DIR}/spiffs.bin")
set(SPIFFSGEN "$ENV{IDF_PATH}/components/spiffs/spiffsgen.py")

# SPIFFS partition size: 0xF0000 = 983040 bytes (960KB)
add_custom_target(spiffs_image
    COMMAND python3 "${SPIFFSGEN}" 
            --page-size 256
            --block-size 4096
            0xF0000
            "${SPIFFS_DIR}" "${SPIFFS_BIN}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM
)

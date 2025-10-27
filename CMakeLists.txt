# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Set custom partition table
set(PARTITION_CSV_PATH "${CMAKE_SOURCE_DIR}/partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(project-name)

# Create SPIFFS image from data folder (if it exists)
# This will automatically flash SPIFFS when you run: idf.py flash
if(EXISTS ${CMAKE_SOURCE_DIR}/data)
    spiffs_create_partition_image(spiffs ${CMAKE_SOURCE_DIR}/data FLASH_IN_PROJECT)
else()
    message(WARNING "data/ folder not found - SPIFFS will not be flashed automatically")
endif()
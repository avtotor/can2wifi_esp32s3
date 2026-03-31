# ESP-IDF project for ESP32-S3
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3_can2wifi)

# После сборки копировать прошивку в firmware/
add_custom_command(
    TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
        ${CMAKE_SOURCE_DIR}/firmware/${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
        ${CMAKE_SOURCE_DIR}/firmware/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
        ${CMAKE_SOURCE_DIR}/firmware/partition-table.bin
    COMMENT "Firmware saved to firmware/"
)

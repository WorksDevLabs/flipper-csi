cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET "esp32s2" CACHE STRING "CSI Radar hardware target")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(flipper_csi_radar)
if(NOT IDF_TARGET STREQUAL "esp32s2")
    message(FATAL_ERROR "This firmware is for ESP32-S2. Run idf.py set-target esp32s2.")
endif()


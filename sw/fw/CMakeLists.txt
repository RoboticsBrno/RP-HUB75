cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.1.1)

set(PICO_BOARD pico2 CACHE STRING "Board type")

# firmware options
option(SR_INPUT_QSPI "Select QSPI as the display input method" OFF)
option(SR_INPUT_USB "Select USB CDC as the display input method" ON)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.0.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(rphub75 C CXX ASM)
pico_sdk_init()

if (SR_INPUT_USB AND SR_INPUT_QSPI)
    message(FATAL_ERROR "Cannot enable both USB CDC and QSPI inputs at once, please select one.")
elseif(SR_INPUT_USB)
    message("-- Using USB CDC as display input")
elseif(SR_INPUT_QSPI)
    message("-- Using QSPI as display input")
else()
    message(FATAL_ERROR "No display input selected.")
endif()

# add firmware subprojects

add_subdirectory(src)

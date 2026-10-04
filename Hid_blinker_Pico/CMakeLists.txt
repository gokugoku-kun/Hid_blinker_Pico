# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
if (NOT PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "D:/Users/n9b01/Documents/Pico/pico-sdk")
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.2.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK 2.2.0 or later is required")
endif()

project(Hid_blinker_Pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
set(TINYUSB_OPT_OS OPT_OS_NONE)
pico_sdk_init()

if (NOT PICO_BOARD STREQUAL "pico_w")
    message(FATAL_ERROR "This firmware targets Raspberry Pi Pico W (PICO_BOARD=pico_w)")
endif()

set(APP_SOURCES
    Hid_blinker_Pico.c
    led_controller.c
    led_protocol.c
    usb_descriptors.c
    usb_transport.c
)
add_executable(Hid_blinker_Pico ${APP_SOURCES})
set_source_files_properties(${APP_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror")

pico_set_program_name(Hid_blinker_Pico "Hid_blinker_Pico")
pico_set_program_version(Hid_blinker_Pico "0.2.0")

pico_enable_stdio_uart(Hid_blinker_Pico 1)
pico_enable_stdio_usb(Hid_blinker_Pico 0)

target_link_libraries(Hid_blinker_Pico
    pico_stdlib
    pico_cyw43_arch_none
    pico_unique_id
    hardware_watchdog
    tinyusb_device
)

# Add the standard include files to the build
target_include_directories(Hid_blinker_Pico PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(Hid_blinker_Pico)


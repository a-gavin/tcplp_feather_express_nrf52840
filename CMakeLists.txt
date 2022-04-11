cmake_minimum_required(VERSION 3.20.0)

set(BOARD "adafruit_feather_nrf52840")

# Set West runner for "flash" and "debug" subcommands
set(BOARD_FLASH_RUNNER "pyocd")
set(BOARD_DEBUG_RUNNER "pyocd")

# Set addtl configuration for USB UART I/O
set(OVERLAY_CONFIG "overlay-usb.conf")
set(DTC_OVERLAY_FILE "usb.overlay")

SET(IS_LISTENER FALSE CACHE STRING "Is listener device (i.e. compile w/ TCP listener code)")

# Swap IP addrs if not listener device
if(NOT IS_LISTENER)
    message(STATUS "IS_LISTENER=FALSE -- Swapping IP addrs")
    string(APPEND OVERLAY_CONFIG " connector.conf")
    message(STATUS "OVERLAY_CONFIG=${OVERLAY_CONFIG}")
endif()

find_package(Zephyr)
project(tcplp_feather_express_nrf52840)

target_sources(app
    PRIVATE
        src/main.c
)

# Enable/disable listener code
if(NOT IS_LISTENER)
    message(STATUS "IS_LISTENER=FALSE -- Disabling listener code")
    target_compile_definitions(app PRIVATE IS_LISTENER=0)
else()
    message(STATUS "IS_LISTENER=TRUE -- Enabling listener code")
    target_compile_definitions(app PRIVATE IS_LISTENER=1)
endif()

set_property(GLOBAL APPEND PROPERTY HEX_FILES_TO_MERGE
    ${app_bootloader_hex}
    ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
    ${app_provision_hex}
)
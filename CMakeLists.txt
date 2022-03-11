cmake_minimum_required(VERSION 3.20.0)

set(BOARD "adafruit_feather_nrf52840")

# Set West runner for "flash" and "debug" subcommands
set(BOARD_FLASH_RUNNER "pyocd")
set(BOARD_DEBUG_RUNNER "pyocd")

find_package(Zephyr)
project(tcplp_feather_express_nrf52840)

target_sources(app
    PRIVATE
        src/main.c
)

set_property(GLOBAL APPEND PROPERTY HEX_FILES_TO_MERGE
    ${app_bootloader_hex}
    ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
    ${app_provision_hex}
)
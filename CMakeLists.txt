# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.5)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(dsf-oscillator-example VERSION 2.3 LANGUAGES C CXX ASM)

add_executable(${PROJECT_NAME}
                dsf-oscillator-pico.cpp
                dsf-oscillator-pico.h
                inc/fix15.h
                example/src/dsf-oscillator-example.cpp
                example/src/dsf-oscillator-example.h
                example/src/tusb_config.h
)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

pico_sdk_init()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/example/src/ PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc $ENV{PICO_SDK_PATH}/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB/src)

add_library(MCP4725_PICO INTERFACE)

target_sources(MCP4725_PICO INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/example/lib/MCP4725_PICO/src/mcp4725/mcp4725.cpp
)

target_include_directories(MCP4725_PICO INTERFACE ${CMAKE_CURRENT_LIST_DIR}/example/lib/MCP4725_PICO/include/mcp4725/)

add_library(pico_encoder INTERFACE)

target_sources(pico_encoder INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/example/lib/pico_encoder/pico_encoder.cpp
)

target_include_directories(pico_encoder INTERFACE ${CMAKE_CURRENT_LIST_DIR}/example/lib/pico_encoder/)

add_library(usb_midi_host INTERFACE)

target_sources(usb_midi_host INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/example/lib/usb_midi_host/usb_midi_host_app_driver.c
  ${CMAKE_CURRENT_LIST_DIR}/example/lib/usb_midi_host/usb_midi_host.c
)

target_include_directories(usb_midi_host INTERFACE ${CMAKE_CURRENT_LIST_DIR}/example/lib/usb_midi_host/)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_i2c
    hardware_adc
    tinyusb_device
    tinyusb_board
    tinyusb_host
    tinyusb_pico_pio_usb
    usb_midi_host
    MCP4725_PICO
    pico_encoder
)
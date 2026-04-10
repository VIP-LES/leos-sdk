# libdriver SX1262 HAL
# Adapts the external/sx1262 library to the selected platform port.

cmake_minimum_required(VERSION 3.15)


set(LEOS_SX126X_PORT "pico" CACHE STRING "Platform port for leos_sx126x (pico or linux)")
set_property(CACHE LEOS_SX126X_PORT PROPERTY STRINGS pico linux)

set(LEOS_SX126X_PORT_SRC sx126x_port_pico.c)
if(LEOS_SX126X_PORT STREQUAL "linux")
    set(LEOS_SX126X_PORT_SRC sx126x_port_linux.c)
elseif(NOT LEOS_SX126X_PORT STREQUAL "pico")
    message(FATAL_ERROR "Unsupported LEOS_SX126X_PORT='${LEOS_SX126X_PORT}'. Expected 'pico' or 'linux'.")
endif()

add_library(leos_sx126x EXCLUDE_FROM_ALL sx126x.c ${LEOS_SX126X_PORT_SRC})

target_include_directories(leos_sx126x
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_sx126x
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_sx126x
    PUBLIC
    sx1262
)
target_link_libraries(leos_sx126x
    PRIVATE
    leos_log
)

if(LEOS_SX126X_PORT STREQUAL "pico")
    target_link_libraries(leos_sx126x
        PUBLIC
        hardware_spi
    )
    target_link_libraries(leos_sx126x
        PRIVATE
        pico_time
        pico_stdlib
        hardware_sync
    )
elseif(LEOS_SX126X_PORT STREQUAL "linux")
    find_library(GPIOD_LIBRARY gpiod REQUIRED)
    target_link_libraries(leos_sx126x
        PRIVATE
        ${GPIOD_LIBRARY}
    )
endif()

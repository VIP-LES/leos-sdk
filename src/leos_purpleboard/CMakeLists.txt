cmake_minimum_required(VERSION 3.15)

include(Pico_BusIO)
include(Adafruit_LTR390)
include(Adafruit_BME680)
include(Adafruit_PM25AQI)
include(Adafruit_TSL2591)

add_library(leos_purpleboard EXCLUDE_FROM_ALL purpleboard.cpp)

target_include_directories(leos_purpleboard
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_purpleboard
    PRIVATE
    leos_log
)
target_link_libraries(leos_purpleboard
    PUBLIC
    Adafruit_LTR390
    Adafruit_PM25AQI
    Adafruit_BME680
    Adafruit_TSL2591
    hardware_i2c
    pico_time
)

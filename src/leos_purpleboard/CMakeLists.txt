cmake_minimum_required(VERSION 3.15)


add_library(leos_purpleboard EXCLUDE_FROM_ALL purpleboard.cpp)

target_include_directories(leos_purpleboard
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_purpleboard
    PRIVATE
    leos_log
    Adafruit_LTR390
    Adafruit_PM25AQI
    Adafruit_TSL2591
)
target_link_libraries(leos_purpleboard
    PUBLIC
    hardware_i2c
)
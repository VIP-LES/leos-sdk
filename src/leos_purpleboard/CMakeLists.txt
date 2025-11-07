cmake_minimum_required(VERSION 3.15)


add_library(leos_purpleboard purpleboard.cpp)

target_include_directories(leos_purpleboard
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_purpleboard
    PRIVATE
    leos_log
    Adafruit_BMP3XX
    Adafruit_LTR390
    Adafruit_PM25AQI
    Adafruit_TSL2561
)

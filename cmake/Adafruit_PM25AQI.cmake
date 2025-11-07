set(Adafruit_PM25AQI_DIR ${LEOS_SDK_ROOT}/external/Adafruit_PM25AQI/)

file(GLOB SRC_FILES ${Adafruit_PM25AQI_DIR}/src/*.c ${Adafruit_PM25AQI_DIR}/src/*.cpp)

add_library(Adafruit_PM25AQI EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_PM25AQI PUBLIC ${Adafruit_PM25AQI_DIR}/src)

target_link_libraries(Adafruit_PM25AQI PUBLIC pico_busio)
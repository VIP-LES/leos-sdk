set(Adafruit_BME680_DIR ${LEOS_SDK_ROOT}/external/Adafruit_BME680/)

file(GLOB SRC_FILES ${Adafruit_BME680_DIR}/*.c ${Adafruit_BME680_DIR}/*.cpp)

add_library(Adafruit_BME680 EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_BME680 PUBLIC ${Adafruit_BME680_DIR})

target_link_libraries(Adafruit_BME680 PUBLIC pico_busio)
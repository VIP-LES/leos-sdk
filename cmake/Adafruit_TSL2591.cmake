set(Adafruit_TSL2591_DIR ${LEOS_SDK_ROOT}/external/Adafruit_TSL2591_Library/)

file(GLOB SRC_FILES ${Adafruit_TSL2591_DIR}/*.c ${Adafruit_TSL2591_DIR}/*.cpp)

add_library(Adafruit_TSL2591 EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_TSL2591 PUBLIC ${Adafruit_TSL2591_DIR})

target_link_libraries(Adafruit_TSL2591 PUBLIC pico_busio)
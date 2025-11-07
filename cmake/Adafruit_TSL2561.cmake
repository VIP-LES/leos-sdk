set(Adafruit_TSL2561_DIR ${LEOS_SDK_ROOT}/external/Adafruit_TSL2561/)

file(GLOB SRC_FILES ${Adafruit_TSL2561_DIR}/*.c ${Adafruit_TSL2561_DIR}/*.cpp)

add_library(Adafruit_TSL2561 EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_TSL2561 PUBLIC ${Adafruit_TSL2561_DIR})

target_link_libraries(Adafruit_TSL2561 PUBLIC pico_busio)
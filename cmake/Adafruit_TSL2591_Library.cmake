set(Adafruit_TSL2591_Library_DIR ${LEOS_SDK_ROOT}/external/Adafruit_TSL2591_Library/)

file(GLOB SRC_FILES ${Adafruit_TSL2591_Library_DIR}/*.c ${Adafruit_TSL2591_Library_DIR}/*.cpp)

add_library(Adafruit_TSL2591_Library EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_TSL2591_Library PUBLIC ${Adafruit_TSL2591_Library_DIR})

target_link_libraries(AdafruitAdafruit_TSL2591_LibraryTSL2561 PUBLIC pico_busio)
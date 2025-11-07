set(ADAFRUIT_BMP3XX_DIR ${PROJECT_SOURCE_DIR}/external/Adafruit_BMP3XX/)

file(GLOB SRC_FILES ${ADAFRUIT_BMP3XX_DIR}/*.c ${ADAFRUIT_BMP3XX_DIR}/*.cpp)

add_library(Adafruit_BMP3XX EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_BMP3XX PUBLIC ${ADAFRUIT_BMP3XX_DIR})

target_link_libraries(Adafruit_BMP3XX PUBLIC pico_busio)
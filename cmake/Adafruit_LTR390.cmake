set(Adafruit_LTR390_DIR ${LEOS_SDK_ROOT}/external/Adafruit_LTR390/)

file(GLOB SRC_FILES ${Adafruit_LTR390_DIR}/*.c ${Adafruit_LTR390_DIR}/*.cpp)

add_library(Adafruit_LTR390 EXCLUDE_FROM_ALL ${SRC_FILES})

target_include_directories(Adafruit_LTR390 PUBLIC ${Adafruit_LTR390_DIR})

target_link_libraries(Adafruit_LTR390 PUBLIC pico_busio)
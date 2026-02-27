# o1heap.cmake
# Manages compile definitions and code generation for this external lib

set(SX1262_DIR ${LEOS_SDK_ROOT}/external/sx1262/)

set(SX1262_C ${SX1262_DIR}/src/driver_sx1262.c)
set(SX1262_H ${SX1262_DIR}/src/driver_sx1262.h)

add_library(sx1262 EXCLUDE_FROM_ALL ${SX1262_C})

target_include_directories(sx1262 PUBLIC ${SX1262_DIR}/src)

set_target_properties(sx1262 PROPERTIES
    PUBLIC_HEADER "${SX1262_H}"
)

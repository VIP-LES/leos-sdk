# o1heap.cmake
# Manages compile definitions and code generation for this external lib

set(O1HEAP_DIR ${PROJECT_SOURCE_DIR}/external/o1heap/o1heap)

set(O1HEAP_C ${O1HEAP_DIR}/o1heap.c)
set(O1HEAP_H ${O1HEAP_DIR}/o1heap.h)

add_library(o1heap ${O1HEAP_C})

target_include_directories(o1heap PUBLIC ${O1HEAP_DIR})

set_target_properties(o1heap PROPERTIES
    PUBLIC_HEADER "${O1HEAP_H}"
)

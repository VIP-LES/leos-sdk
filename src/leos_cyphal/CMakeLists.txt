# Prevent double including the external targets
if(NOT TARGET libcanard)
    include(libcanard)
endif()
if(NOT TARGET o1heap)
    include(o1heap)
endif()

add_library(leos_cyphal STATIC node.cpp publisher.cpp subscriber.cpp)
target_compile_features(leos_cyphal PUBLIC cxx_std_17)
target_include_directories(leos_cyphal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(leos_cyphal PUBLIC leos_mcp251xfd libcanard o1heap)
target_link_libraries(leos_cyphal PRIVATE hardware_gpio leos_log pico_time pico_unique_id)

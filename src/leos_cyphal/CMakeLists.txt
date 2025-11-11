add_library(leos_cyphal INTERFACE)
target_include_directories(leos_cyphal INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(leos_cyphal INTERFACE libcanard)

add_subdirectory(node)
add_subdirectory(transport)
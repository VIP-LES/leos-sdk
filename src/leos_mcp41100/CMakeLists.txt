add_library(leos_mcp41100 STATIC mcp41100.cpp)
target_compile_features(leos_mcp41100 PUBLIC cxx_std_17)
target_include_directories(leos_mcp41100 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(leos_mcp41100 PUBLIC hardware_spi PRIVATE pico_stdlib)

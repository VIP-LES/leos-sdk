cmake_minimum_required(VERSION 3.15)


add_library(leos_lora EXCLUDE_FROM_ALL lora.c hal.c)

target_include_directories(leos_lora
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_lora
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_lora
    PUBLIC
    sx126x_driver
    hardware_spi
    
)
target_link_libraries(leos_lora
    PRIVATE
    leos_log
    pico_time
    pico_stdlib
)

# if(LEOS_SX126X_PORT STREQUAL "pico")
#     target_link_libraries(leos_
#         PUBLIC
#         hardware_spi
#     )
#     target_link_libraries(leos_sx126x
#         PRIVATE
#         pico_time
#         pico_stdlib
#         hardware_sync
#     )
# elseif(LEOS_SX126X_PORT STREQUAL "linux")
#     find_library(GPIOD_LIBRARY gpiod REQUIRED)
#     target_link_libraries(leos_sx126x
#         PRIVATE
#         ${GPIOD_LIBRARY}
#     )
# endif()

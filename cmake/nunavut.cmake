# Usage Example:
#   cyphal_generate_types(
#       TARGET firmware
#       DSDL_DIR ${PROJECT_SOURCE_DIR}/external/publictypes/reg
#       DSDL_LOOKUP_DIRS ${PROJECT_SOURCE_DIR}/external/publictypes/uavcan
#   )

find_program(NNVG_EXE nnvg PATHS "$ENV{HOME}/.local/bin" "${CMAKE_SOURCE_DIR}/.venv/bin" NO_DEFAULT_PATH)
find_program(NNVG_EXE nnvg PATHS "$ENV{HOME}/.local/bin" "${CMAKE_SOURCE_DIR}/.vip-venv/bin" NO_DEFAULT_PATH)
find_program(NNVG_EXE nnvg)
if(NOT NNVG_EXE)
    message(FATAL_ERROR "nnvg not found. Install with: pip install nunavut")
endif()

function(cyphal_generate_types)
    cmake_parse_arguments(
        CYPHAL
        ""
        "TARGET;DSDL_DIR"
        "DSDL_LOOKUP_DIRS"
        ${ARGN}
    )

    if(NOT CYPHAL_TARGET)
        message(FATAL_ERROR "cyphal_generate_types: TARGET is required.")
    endif()

    if(NOT CYPHAL_DSDL_DIR)
        message(FATAL_ERROR "cyphal_generate_types: DSDL_DIR (root namespace) is required.")
    endif()

    # Build lookup args
    set(LOOKUP_ARGS "")
    foreach(LU IN LISTS CYPHAL_DSDL_LOOKUP_DIRS)
        list(APPEND LOOKUP_ARGS --lookup-dir ${LU})
    endforeach()

    # Derive a safe unique target name from path
    file(RELATIVE_PATH REL_ROOT ${PROJECT_SOURCE_DIR} ${CYPHAL_DSDL_DIR})
    string(REPLACE "/" "_" SAFE_NAME "${REL_ROOT}")
    string(REPLACE "." "_" SAFE_NAME "${SAFE_NAME}")
    set(GEN_TARGET generate_cyphal_types_${CYPHAL_TARGET}_${SAFE_NAME})

    # Output directory shared per firmware target
    set(CYPHAL_DSDL_GEN_DIR ${PROJECT_BINARY_DIR}/generated_dsdl/${CYPHAL_TARGET})
    file(MAKE_DIRECTORY ${CYPHAL_DSDL_GEN_DIR})

    # Stamp marks success of this invocation
    set(CYPHAL_DSDL_STAMP ${CYPHAL_DSDL_GEN_DIR}/generated_${SAFE_NAME}.stamp)

    add_custom_command(
        OUTPUT ${CYPHAL_DSDL_STAMP}
        COMMAND ${NNVG_EXE}
            --target-language c
            # --enable-serialization-asserts
            ${CYPHAL_DSDL_DIR}            # <== Root namespace (positional)
            ${LOOKUP_ARGS}                # <== Lookup namespaces
            --outdir ${CYPHAL_DSDL_GEN_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${CYPHAL_DSDL_STAMP}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        DEPENDS ${CYPHAL_DSDL_DIR} ${CYPHAL_DSDL_LOOKUP_DIRS}
        COMMENT "Generating Cyphal DSDL for ${REL_ROOT} → ${CYPHAL_DSDL_GEN_DIR}"
        VERBATIM
    )

    add_custom_target(${GEN_TARGET}
        DEPENDS ${CYPHAL_DSDL_STAMP}
    )

    target_include_directories(${CYPHAL_TARGET} PUBLIC ${CYPHAL_DSDL_GEN_DIR})
    add_dependencies(${CYPHAL_TARGET} ${GEN_TARGET})
endfunction()

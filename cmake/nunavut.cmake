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

    # Derive a stable safe name for the DSDL directory
    get_filename_component(DSDL_BASENAME "${CYPHAL_DSDL_DIR}" NAME)
    string(REPLACE "." "_" DSDL_SAFE "${DSDL_BASENAME}")
    string(REPLACE "-" "_" DSDL_SAFE "${DSDL_SAFE}")
    string(REPLACE "/" "_" DSDL_SAFE "${DSDL_SAFE}")

    # Add a short 6-char hash for uniqueness (based on absolute path)
    string(MD5 DSDL_HASH "${CYPHAL_DSDL_DIR}")
    string(SUBSTRING "${DSDL_HASH}" 0 6 DSDL_HASH_SHORT)

    # Compose a final safe name and target
    set(SAFE_NAME "${DSDL_SAFE}_${DSDL_HASH_SHORT}")
    set(GEN_TARGET "generate_cyphal_types_${CYPHAL_TARGET}_${SAFE_NAME}")

    # Output directory shared per firmware target
    set(CYPHAL_DSDL_GEN_DIR ${PROJECT_BINARY_DIR}/generated_dsdl/${CYPHAL_TARGET})
    file(MAKE_DIRECTORY ${CYPHAL_DSDL_GEN_DIR})

    # Stamp marks success of this invocation
    set(CYPHAL_DSDL_STAMP ${CYPHAL_DSDL_GEN_DIR}/generated_${SAFE_NAME}.stamp)

    add_custom_command(
        OUTPUT ${CYPHAL_DSDL_STAMP}
        COMMAND ${NNVG_EXE}
            --target-language c
            --allow-unregulated-fixed-port-id
            # --enable-serialization-asserts
            ${CYPHAL_DSDL_DIR}            # <== Root namespace (positional)
            ${LOOKUP_ARGS}                # <== Lookup namespaces
            --outdir ${CYPHAL_DSDL_GEN_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${CYPHAL_DSDL_STAMP}
        DEPENDS ${CYPHAL_DSDL_DIR} ${CYPHAL_DSDL_LOOKUP_DIRS}
        COMMENT "Generating Cyphal DSDL for ${REL_ROOT} -> ${CYPHAL_DSDL_GEN_DIR}"
        VERBATIM
    )

    add_custom_target(${GEN_TARGET}
        DEPENDS ${CYPHAL_DSDL_STAMP}
    )

    target_include_directories(${CYPHAL_TARGET} PUBLIC ${CYPHAL_DSDL_GEN_DIR})
    add_dependencies(${CYPHAL_TARGET} ${GEN_TARGET})
endfunction()
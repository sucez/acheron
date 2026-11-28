execute_process(COMMAND git rev-parse --verify HEAD WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE ACHERON_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ACHERON_COMMIT MATCHES "^[0-9a-f]+$")
    if(NOT ACHERON_CHANNEL STREQUAL "development")
        message(FATAL_ERROR "Release builds require a Git checkout")
    endif()
    set(ACHERON_COMMIT unknown)
endif()
configure_file("${SOURCE_DIR}/cmake/BuildInfo.cpp.in" "${OUTPUT}" @ONLY)

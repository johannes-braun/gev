execute_process(COMMAND ${GLSLC_EXECUTABLE} ${FROM_FILE} -mfmt=c -o -
    OUTPUT_VARIABLE EMPLACE_SOURCE_HERE
    ERROR_VARIABLE COMPILE_ERROR)
if(COMPILE_ERROR)
    message(FATAL_ERROR "${COMPILE_ERROR}")
endif()

configure_file("${OVER_FILE}" "${TO_FILE}" @ONLY)

execute_process(COMMAND ${GLSLC_EXECUTABLE} ${FROM_FILE} -o "${TO_FILE}.spv"
    ERROR_VARIABLE COMPILE_ERROR)

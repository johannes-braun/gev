include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(FetchContent)

find_program(GLSLC_EXECUTABLE glslc)
set(CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})

macro(build_shader file prefix list shader_paths_code_lines)
    set(FILE_REL ${file})
    file(TO_CMAKE_PATH "${FILE_REL}" FILE_REL)
    string(REPLACE "." "_" FILE_REL_ALL "${FILE_REL}")
    string(REPLACE " " "_" FILE_REL_ALL "${FILE_REL_ALL}")
    string(REPLACE "\\" "/" FILE_REL_ALL "${FILE_REL_ALL}")
    string(REPLACE "/" ";" FILE_REL_ALL "${FILE_REL_ALL}")

    set(FILE_REL_ALL_REV ${FILE_REL_ALL})
    list(REVERSE FILE_REL_ALL_REV)
    list(GET FILE_REL_ALL_REV 0 FILE_REL_ALL)
    list(REMOVE_AT FILE_REL_ALL_REV 0)
    list(REVERSE FILE_REL_ALL_REV)

    string(JOIN "::" NAMESPACE ${FILE_REL_ALL_REV})

    string(JOIN "_" FILE_REL_ALL ${FILE_REL_ALL})
    string(TOLOWER "${FILE_REL_ALL}" FILE_REL_ALL)
    string(JOIN "_" VAR_UNDER ${prefix} ${FILE_REL_ALL})
    set(FROM_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILE_REL})

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${prefix})
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${prefix}/${FILE_REL}.spv"
      COMMAND ${GLSLC_EXECUTABLE} "${FROM_FILE}" -O -o "${CMAKE_CURRENT_BINARY_DIR}/${prefix}/${FILE_REL}.spv"
      COMMENT "Generating shader ${FILE_REL}.spv"
      DEPENDS "${FROM_FILE}"
      VERBATIM
    )
    list(APPEND ${list} "${CMAKE_CURRENT_BINARY_DIR}/${prefix}/${FILE_REL}.spv")
    list(APPEND ${shader_paths_code_lines} "namespace ${NAMESPACE} { constexpr static char const ${FILE_REL_ALL}[] = \"${prefix}/${FILE_REL}.spv\"§ }\n")
endmacro()

function(compile_shaders target_name)
  set(SPIRV_FILES)
  set(SPIRV_FILES_REL)
  foreach(SOURCE ${ARGN})
      get_filename_component(EXTENSION ${SOURCE} LAST_EXT)
      set(SOURCE_NAME ${SOURCE})

      set(FILE_REL ${SOURCE})
      if(IS_ABSOLUTE ${FILE_REL})
          file(RELATIVE_PATH FILE_REL ${CMAKE_CURRENT_SOURCE_DIR} ${FILE_REL})
      endif()

      BUILD_SHADER(${FILE_REL} ${target_name} SPIRV_FILES SPIRV_FILES_REL)
  endforeach()
  
  string(REPLACE ";" "" SPIRV_FILES_REL "${SPIRV_FILES_REL}")
  string(REPLACE "§" ";" SPIRV_FILES_REL "${SPIRV_FILES_REL}")
  set(FILES_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/${target_name}/${target_name}_files.hpp")
  file(WRITE "${FILES_HEADER}" "#pragma once\nnamespace ${target_name} {\n")
  file(APPEND "${FILES_HEADER}" "${SPIRV_FILES_REL}")
  file(APPEND "${FILES_HEADER}" "}\n")

  add_custom_target(${target_name}deps ALL DEPENDS ${SPIRV_FILES})
  add_library(${target_name} INTERFACE)
  target_include_directories(${target_name} INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/${target_name})
  add_dependencies(${target_name} INTERFACE ${target_name}deps)
endfunction()

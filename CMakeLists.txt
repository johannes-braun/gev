cmake_minimum_required(VERSION 3.27)

project(GEV
  VERSION 1.0
  LANGUAGES CXX)
  
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(VULKAN_DIR $ENV{VULKAN_SDK})
message("-- Found Vulkan SDK at: ${VULKAN_DIR}")
find_library(VULKAN_LIB NAMES vulkan-1 PATHS ${VULKAN_DIR}/Lib)

set(${VK_LIBRARIES} ${VULKAN_LIB})
macro(find_vk_library name)
  find_library(${name}_LIB_DEBUG NAMES ${name} PATHS ${VULKAN_DIR}/Lib)
  find_library(${name}_LIB_RELEASE NAMES ${name} PATHS ${VULKAN_DIR}/Lib)
  add_library(${name} STATIC IMPORTED)
  set_target_properties(${name} PROPERTIES IMPORTED_LOCATION_DEBUG ${${name}_LIB_DEBUG})
  set_target_properties(${name} PROPERTIES IMPORTED_LOCATION_RELWITHDEBINFO ${${name}_LIB_RELEASE})
  set_target_properties(${name} PROPERTIES IMPORTED_LOCATION_RELEASE ${${name}_LIB_RELEASE})
  list(APPEND VK_LIBRARIES ${name})
endmacro()

find_vk_library(shaderc_combined)
find_vk_library(shaderc)
find_vk_library(shaderc_util)
find_vk_library(SPIRV)
include(cmake/AddShaderPackage.cmake)

find_package(glfw3 REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)

add_library(gev.vulkan INTERFACE)
target_include_directories(gev.vulkan INTERFACE ${VULKAN_DIR}/Include)
target_link_libraries(gev.vulkan INTERFACE glfw)
target_link_libraries(gev.vulkan INTERFACE unofficial::vulkan-memory-allocator::vulkan-memory-allocator ${VK_LIBRARIES})
target_compile_definitions(gev.vulkan INTERFACE VK_NO_PROTOTYPES VULKAN_HPP_STORAGE_SHARED)

define_property(TARGET PROPERTY 
  SPIRV_FILES)
define_property(TARGET PROPERTY 
  SPIRV_FILES_REL)
  
add_subdirectory(libraries/rnu)
add_subdirectory(packages/imgui)
add_subdirectory(packages/core)
add_subdirectory(packages/scenery)
add_subdirectory(packages/audio)
add_subdirectory(packages/game)

add_subdirectory(executables/01_test)
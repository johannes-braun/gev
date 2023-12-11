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

add_library(gev_vulkan_packages INTERFACE)
target_include_directories(gev_vulkan_packages INTERFACE ${VULKAN_DIR}/Include)
target_link_libraries(gev_vulkan_packages INTERFACE glfw)
target_link_libraries(gev_vulkan_packages INTERFACE unofficial::vulkan-memory-allocator::vulkan-memory-allocator ${VK_LIBRARIES})
target_compile_definitions(gev_vulkan_packages INTERFACE VK_NO_PROTOTYPES VULKAN_HPP_STORAGE_SHARED)

#Manually Create ImGui Library
add_library(gev.imgui SHARED)
target_sources(gev.imgui PRIVATE
  "libraries/imgui/imgui.cpp"
  "libraries/imgui/imgui.cpp"
  "libraries/imgui/imgui_demo.cpp"
  "libraries/imgui/imgui_draw.cpp"
  "libraries/imgui/backends/imgui_impl_vulkan.cpp"
  "libraries/imgui/backends/imgui_impl_glfw.cpp"
  "libraries/imgui/imgui_tables.cpp"
  "libraries/imgui/imgui_widgets.cpp")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/imgui/include/gev/imgui)
file(COPY
    "libraries/imgui/imgui.h"
    "libraries/imgui/imconfig.h"
    "libraries/imgui/backends/imgui_impl_vulkan.h"
    "libraries/imgui/backends/imgui_impl_glfw.h"
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/imgui/include/gev/imgui)
target_include_directories(gev.imgui PRIVATE BEFORE "libraries/imgui")
target_include_directories(gev.imgui PUBLIC BEFORE ${CMAKE_CURRENT_BINARY_DIR}/imgui/include)
target_link_libraries(gev.imgui PUBLIC gev_vulkan_packages glfw)
#End Create ImGui Library

add_subdirectory(libraries/rnu)
add_subdirectory(packages/core)
add_subdirectory(packages/scenery)
add_subdirectory(packages/audio)
add_subdirectory(executables/01_test)
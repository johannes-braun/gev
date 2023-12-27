set(GEV_CURRENT_LIBRARY gev.imgui)

set(IMGUI_FILES_PATH "${CMAKE_SOURCE_DIR}/libraries/imgui")

#Manually Create ImGui Library
add_library(gev.imgui SHARED)
target_sources(gev.imgui PRIVATE
  "${IMGUI_FILES_PATH}/imgui.cpp"
  "${IMGUI_FILES_PATH}/imgui.cpp"
  "${IMGUI_FILES_PATH}/imgui_demo.cpp"
  "${IMGUI_FILES_PATH}/imgui_draw.cpp"
  "${IMGUI_FILES_PATH}/backends/imgui_impl_vulkan.cpp"
  "${IMGUI_FILES_PATH}/backends/imgui_impl_glfw.cpp"
  "${IMGUI_FILES_PATH}/imgui_tables.cpp"
  "${IMGUI_FILES_PATH}/imgui_widgets.cpp"
  "src/imgui_extra.cpp")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/imgui/include/gev/imgui)
file(COPY
    "${IMGUI_FILES_PATH}/imgui.h"
    "${IMGUI_FILES_PATH}/imgui_internal.h"
    "${IMGUI_FILES_PATH}/imstb_textedit.h"
    "${IMGUI_FILES_PATH}/imconfig.h"
    "${IMGUI_FILES_PATH}/backends/imgui_impl_vulkan.h"
    "${IMGUI_FILES_PATH}/backends/imgui_impl_glfw.h"
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/imgui/include/gev/imgui)
target_include_directories(gev.imgui PRIVATE BEFORE "${IMGUI_FILES_PATH}")
target_include_directories(gev.imgui PUBLIC BEFORE 
  ${CMAKE_CURRENT_BINARY_DIR}/imgui/include
  include)
target_link_libraries(gev.imgui PUBLIC gev.vulkan glfw)
#End Create ImGui Library


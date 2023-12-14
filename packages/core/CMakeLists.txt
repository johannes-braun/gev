set(GEV_CURRENT_LIBRARY gev.core)

add_library(${GEV_CURRENT_LIBRARY} SHARED)
target_compile_definitions(${GEV_CURRENT_LIBRARY} PRIVATE GLFW_INCLUDE_NONE VULKAN_HPP_STORAGE_SHARED_EXPORT)
target_compile_definitions(${GEV_CURRENT_LIBRARY} PUBLIC VK_NO_PROTOTYPES VULKAN_HPP_STORAGE_SHARED)
target_compile_features(${GEV_CURRENT_LIBRARY} PUBLIC cxx_std_23)
target_include_directories(${GEV_CURRENT_LIBRARY} PUBLIC include)
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC gev.vulkan rnu::rnu)

target_sources(${GEV_CURRENT_LIBRARY} PRIVATE
  "src/engine.cpp"
  "src/vma.cpp"
  "src/buffer.cpp"
  "src/image.cpp"
  "src/pipeline.cpp"
  "src/descriptors.cpp"
  "src/rethink_sans.cpp"
  "src/logger.cpp")
  
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC 
  gev.imgui
  gev.audio)
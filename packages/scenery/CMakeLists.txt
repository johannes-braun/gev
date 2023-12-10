set(GEV_CURRENT_LIBRARY gev.scenery)

add_library(${GEV_CURRENT_LIBRARY} SHARED)
target_compile_features(${GEV_CURRENT_LIBRARY} PUBLIC cxx_std_23)
target_include_directories(${GEV_CURRENT_LIBRARY} PUBLIC include)
target_compile_definitions(${GEV_CURRENT_LIBRARY} PRIVATE GLFW_INCLUDE_NONE VULKAN_HPP_STORAGE_SHARED_EXPORT)
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC rnu::rnu)
target_sources(${GEV_CURRENT_LIBRARY} PRIVATE
  "src/entity.cpp" "src/entity_manager.cpp" "src/component.cpp" "src/transform.cpp")

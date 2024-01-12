set(GEV_CURRENT_LIBRARY gev.game)

add_library(${GEV_CURRENT_LIBRARY} SHARED)
target_compile_features(${GEV_CURRENT_LIBRARY} PUBLIC cxx_std_23)
target_include_directories(${GEV_CURRENT_LIBRARY} PUBLIC include)
target_compile_definitions(${GEV_CURRENT_LIBRARY} PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC gev.core gev.audio gev.scenery gev.imgui)
target_sources(${GEV_CURRENT_LIBRARY} PRIVATE
  "src/renderer.cpp"
  "src/blur.cpp"
  "src/cutoff.cpp"
  "src/mesh.cpp"
  "src/mesh_renderer.cpp"
  "src/texture.cpp"
  "src/camera.cpp"
  "src/material.cpp"
  "src/distance_field_generator.cpp"
  "src/distance_field.cpp"
  "src/layouts.cpp"
  "src/distance_field_holder.cpp"
  "src/mesh_batch.cpp"
  "src/shadow_map_holder.cpp"
  "src/shader.cpp"
  "src/sync_buffer.cpp"
  "src/cascaded_shadow_mapping.cpp"
  "src/addition.cpp"
  "src/render_target_2d.cpp"
  "src/samplers.cpp"
  "src/tonemap.cpp" "src/vignette.cpp" "src/film_grain.cpp")

compile_shaders(gev_game_shaders
  "shaders/generate_distance_field.comp"
  "shaders/depth_only.frag"
  "shaders/shader2.frag"
  "shaders/shader2.vert"
  "shaders/shader2_rig.vert"
  "shaders/blur.comp"
  "shaders/cutoff.comp"
  "shaders/tonemap.comp"
  "shaders/addition.comp"
  "shaders/vignette.comp"
  "shaders/film_grain.comp")
target_link_libraries(gev.game PUBLIC gev_game_shaders)

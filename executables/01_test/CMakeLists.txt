add_executable(01_test 
  "01_test.cpp" 
  "camera.cpp"
  "object.cpp" 
  "texture.cpp" "ddf_generator.cpp")
target_link_libraries(01_test PRIVATE gev.core gev.scenery)
add_custom_command(TARGET 01_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:01_test> $<TARGET_FILE_DIR:01_test>
  COMMAND_EXPAND_LISTS
  )
add_custom_command(TARGET 01_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/res" $<TARGET_FILE_DIR:01_test>/res
  COMMAND_EXPAND_LISTS
  )

compile_shaders(
  test_shaders
  "shaders/shader.vert"
  "shaders/shader.frag"
  "shaders/environment.vert"
  "shaders/environment.frag"
  "shaders/generate_distance_field.comp"
  "shaders/draw_ddf.vert"
  "shaders/draw_ddf.frag"
)
target_link_libraries(01_test PRIVATE test_shaders rnu::rnu)

find_package(Stb REQUIRED)
target_include_directories(01_test PRIVATE ${Stb_INCLUDE_DIR})
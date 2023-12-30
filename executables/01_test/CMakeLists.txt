if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set (WIN32_RESOURCES test01.rc)
endif()

add_executable(01_test 
  "01_test.cpp" 
  "components/camera_component.cpp"
  "components/camera_controller_component.cpp"
  "components/renderer_component.cpp"
  ${WIN32_RESOURCES}    
  "components/shadow_map_component.cpp"
  "components/bone_component.cpp"
  "components/skin_component.cpp"
  "components/sound_component.cpp"
  "environment_shader.cpp"
  "gltf_loader.cpp"
  "components/debug_ui_component.cpp" "components/ground_component.cpp" "components/remote_controller_component.cpp")
target_link_libraries(01_test PRIVATE gev.core gev.scenery gev.imgui gev.audio gev.game)
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
  "shaders/environment.vert"
  "shaders/environment.frag"
  "shaders/generate_distance_field.comp"
  "shaders/draw_ddf.vert"
  "shaders/draw_ddf.frag"
  "shaders/grass.vert"
  "shaders/grass.frag"
)
target_link_libraries(01_test PRIVATE test_shaders rnu::rnu)

find_package(Stb REQUIRED)
target_include_directories(01_test PRIVATE ${Stb_INCLUDE_DIR})

install(TARGETS 01_test
    RUNTIME_DEPENDENCIES
    # exclude whatever these are?
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    # exclude 32 bit versions of dependencies maybe?
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
    RUNTIME DESTINATION .)
install(DIRECTORY res DESTINATION .)
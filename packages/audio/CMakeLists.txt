set(GEV_CURRENT_LIBRARY gev.audio)

find_package(OpenAL CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(flac CONFIG REQUIRED)
find_package(mp3lame CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(mpg123 CONFIG REQUIRED)

add_library(${GEV_CURRENT_LIBRARY} SHARED)
target_compile_features(${GEV_CURRENT_LIBRARY} PUBLIC cxx_std_23)
target_include_directories(${GEV_CURRENT_LIBRARY} PUBLIC include)
target_compile_definitions(${GEV_CURRENT_LIBRARY} PRIVATE GLFW_INCLUDE_NONE VULKAN_HPP_STORAGE_SHARED_EXPORT)
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC 
  rnu::rnu 
  OpenAL::OpenAL 
  SndFile::sndfile 
  Vorbis::vorbisenc 
  FLAC::FLAC 
  mp3lame::mp3lame 
  Opus::opus
  MPG123::libmpg123)
target_sources(${GEV_CURRENT_LIBRARY} PRIVATE
  "src/audio.cpp"
  "src/audio_host.cpp" 
  "src/sound.cpp"
  "src/playback.cpp"
  "src/source_allocator.cpp")

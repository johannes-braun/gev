set(GEV_CURRENT_LIBRARY gev.res)

add_library(${GEV_CURRENT_LIBRARY} SHARED)
target_compile_features(${GEV_CURRENT_LIBRARY} PUBLIC cxx_std_23)
target_include_directories(${GEV_CURRENT_LIBRARY} PUBLIC include)
target_link_libraries(${GEV_CURRENT_LIBRARY} PUBLIC rnu::rnu)
target_sources(${GEV_CURRENT_LIBRARY} PRIVATE
  "src/serializer.cpp")

find_package(ZLIB REQUIRED)
target_link_libraries(gev.res PUBLIC ZLIB::ZLIB)
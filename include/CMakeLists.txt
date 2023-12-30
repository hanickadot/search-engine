add_library(crawler)

target_sources(crawler PUBLIC crawler/strip-tags.hpp PRIVATE crawler/strip-tags.cpp)

target_compile_features(crawler PUBLIC cxx_std_23)
target_include_directories(crawler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

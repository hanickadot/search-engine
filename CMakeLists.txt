cmake_minimum_required(VERSION 3.25)

project(CRAWLER VERSION 1.0 LANGUAGES CXX)

if (PROJECT_IS_TOP_LEVEL)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/catch2/CMakeLists.txt)
	message(STATUS "Using local copy of catch2")
	#set(CATCH_CONFIG_FAST_COMPILE ON)
	add_subdirectory(external/catch2 EXCLUDE_FROM_ALL SYSTEM)
endif()

# don't include pedantic before catch
include(colors)
include(pedantic)

option(CRAWLER_TESTS "Enable testing" ON)

if (CRAWLER_TESTS)
	option(CRAWLER_COVERAGE "Enable test-coverage" OFF)

	if (CRAWLER_COVERAGE)
		include(coverage)
		enable_coverage()
	else()
		message(STATUS "Test coverage measurement is OFF")
	endif()
	
	option(CRAWLER_FUZZ "Enable fuzzing targets" OFF)

	if (CRAWLER_FUZZ)
		include(fuzzing)
	else()
		message(STATUS "Building fuzz targets is OFF")
	endif()

	#add_subdirectory(tests)
else()
	message(STATUS "Tests are disabled")
endif()

option(CRAWLER_EXAMPLES "Build examples" ON)

if (CRAWLER_EXAMPLES)
	#add_subdirectory(examples)
endif()


endif()

add_subdirectory(external/ctre)
add_subdirectory(external/co_curl)
add_subdirectory(include)


add_executable(build-index build-index.cpp)
target_link_libraries(build-index co_curl ctre crawler)
target_compile_features(build-index PUBLIC cxx_std_23)

add_executable(strip strip.cpp)
target_link_libraries(strip crawler)
target_compile_features(strip PUBLIC cxx_std_23)




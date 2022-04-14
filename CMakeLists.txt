cmake_minimum_required(VERSION 3.10)

project(threadpool, VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	IF(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /WX -Od ")
	ELSE()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Wall -Wextra -pedantic -Werror")
	ENDIF()
	
ELSEIF("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	IF(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc -O2")
	ELSE()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g1")
	ENDIF()	
ELSE()	
	message("Undefined build type... The default 'CMAKE_CXX_FLAGS' will be used")
ENDIF()


########## UNIT TESTS #########################################################
# GoogleTest requires at least C++11
# set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

###############################################################################

enable_testing()

add_executable(threadpool_tests threadpool_tests.cpp threadpool.hpp)

target_link_libraries(threadpool_tests gtest_main)

include(GoogleTest)
gtest_discover_tests(threadpool_tests)
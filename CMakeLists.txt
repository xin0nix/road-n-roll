cmake_minimum_required(VERSION 3.28.3)
project(Core)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_SCAN_FOR_INCLUDES OFF)
# Specify the Boost libraries you need, including json
set(BOOST_INCLUDE_LIBRARIES asio beast json uuid url)
set(BOOST_ENABLE_CMAKE ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

include(FetchContent)

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz
)
FetchContent_MakeAvailable(Boost)

add_subdirectory(core)

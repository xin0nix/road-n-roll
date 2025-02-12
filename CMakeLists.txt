cmake_minimum_required(VERSION 3.28.3)
project(Core)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_SCAN_FOR_INCLUDES OFF)

# Specify the Boost libraries you need, including json
set(BOOST_INCLUDE_LIBRARIES asio beast json uuid)
set(BOOST_ENABLE_CMAKE ON)

include(FetchContent)

# Fetch Boost (including Boost.JSON)
FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.85.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Boost)

# Fetch Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

# Fetch Google Mock
FetchContent_Declare(
  gmock
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  SOURCE_SUBDIR googletest-1.15.2/googlemock
)
FetchContent_MakeAvailable(gmock)

enable_testing()

add_subdirectory(core)

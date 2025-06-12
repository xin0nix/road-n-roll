# Fetch Google Test
FetchContent_Declare(
    gtest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(gtest)

# Fetch Google Mock
FetchContent_Declare(
  gmock
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  SOURCE_SUBDIR googletest-1.15.2/googlemock
)
FetchContent_MakeAvailable(gmock)

enable_testing()

include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(lib)

add_executable(ServerTest)

if(WIN32)
    target_compile_options(ServerTest PRIVATE /EHsc)
endif()

target_link_libraries(ServerTest
    PUBLIC
    Router
    RouterTest
    DatabaseTest
    GTest::gtest_main
    GTest::gmock_main
    Boost::url
    Boost::beast
    Boost::json
    Boost::uuid
)

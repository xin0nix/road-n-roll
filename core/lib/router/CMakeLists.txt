add_library(Router OBJECT "router.cpp")

target_link_libraries(Router PUBLIC
    Boost::url
    Boost::beast
)

target_include_directories(Router PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

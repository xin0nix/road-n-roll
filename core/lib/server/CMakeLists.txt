add_library(Server OBJECT "server.cpp")

target_link_libraries(Server PUBLIC
    Router
    Boost::url
    Boost::beast
    Boost::log
    Boost::json
    Boost::uuid
)

target_include_directories(Server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

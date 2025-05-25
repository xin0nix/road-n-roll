add_library(GameStore OBJECT "game_store.cpp")

target_link_libraries(GameStore PUBLIC
    Server
    Boost::url
    Boost::beast
    Boost::log
    Boost::json
    Boost::uuid
)

target_include_directories(GameStore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

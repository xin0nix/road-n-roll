add_library(GameStore OBJECT "game_store.cpp")

target_link_libraries(GameStore PUBLIC
    Database
    Server
    Boost::beast
    Boost::json
    Boost::log
    Boost::url
    Boost::uuid
    Boost::hana
    libpqxx::pqxx
)

target_include_directories(GameStore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

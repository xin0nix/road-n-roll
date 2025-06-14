add_library(Database OBJECT "database.cpp")

target_link_libraries(Database PRIVATE
    Boost::hana
    Boost::log
    libpqxx::pqxx
)

target_include_directories(Database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

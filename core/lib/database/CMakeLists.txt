add_library(Database OBJECT "database.cpp" "serializer.cpp")

target_link_libraries(Database PRIVATE
    Boost::hana
    Boost::log
    Boost::uuid
    libpqxx::pqxx
)

target_include_directories(Database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

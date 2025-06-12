add_library(Database OBJECT "database.cpp")

target_link_libraries(Database PUBLIC
    Boost::hana
)

target_include_directories(Database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

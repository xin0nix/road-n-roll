add_library(DatabaseTest OBJECT
    serializer_test.cpp
)

target_link_libraries(DatabaseTest PRIVATE Database
    GTest::gtest
    GTest::gmock
    Boost::hana
)
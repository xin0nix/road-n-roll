add_library(RouterTest OBJECT
    router_test.cpp
    segment_pattern_rule_test.cpp
)

target_link_libraries(RouterTest PRIVATE Router
    GTest::gtest
    GTest::gmock
    Boost::url
    Boost::beast
    Boost::json
    Boost::uuid
)
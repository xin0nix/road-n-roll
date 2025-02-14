add_library(Router OBJECT router.cpp)
target_link_libraries(Router PRIVATE Boost::url Boost::beast)

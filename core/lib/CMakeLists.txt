file(GLOB CPP_SOURCES "*.cpp")

add_subdirectory(router)

add_library(CoreLib OBJECT ${CPP_SOURCES})

target_link_libraries(CoreLib PUBLIC
    Boost::asio
    Boost::beast
    Boost::json
    Boost::url
    Boost::uuid
)

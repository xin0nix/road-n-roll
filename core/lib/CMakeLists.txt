file(GLOB CPP_SOURCES "*.cpp")

add_subdirectory(router)

add_library(CoreLib OBJECT ${CPP_SOURCES})

if(WIN32)
    target_compile_options(CoreLib PRIVATE /EHsc)
endif()

target_link_libraries(CoreLib PUBLIC
    Router
    Boost::asio
    Boost::beast
    Boost::json
    Boost::url
    Boost::uuid
)

add_executable(core main.cpp)

target_link_libraries(core PRIVATE
    Boost::asio
    Boost::beast
    Boost::json
    Boost::uuid
)
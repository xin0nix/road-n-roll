add_subdirectory(database)
add_subdirectory(game_store)
add_subdirectory(router)
add_subdirectory(server)

add_library(CoreLib INTERFACE)

if(WIN32)
    target_compile_options(CoreLib PRIVATE /EHsc)
endif()

target_link_libraries(CoreLib INTERFACE
    Router
    Server
    GameStore
)

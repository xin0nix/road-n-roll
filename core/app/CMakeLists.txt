add_executable(CoreApp main.cpp)

if(WIN32)
    target_compile_options(CoreApp PRIVATE /EHsc)
endif()

target_link_libraries(CoreApp PRIVATE
    CoreLib
)

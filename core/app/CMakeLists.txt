add_executable(CoreApp main.cpp)

if(WIN32)
    target_compile_options(CoreApp PRIVATE /EHsc)
endif()

target_link_libraries(CoreApp PRIVATE
    CoreLib
)

# Find Python and ensure we can create virtual environments
find_package(Python REQUIRED COMPONENTS Interpreter)

add_custom_target(build_wheel ALL
    COMMAND ${Python_EXECUTABLE} -m venv ${CMAKE_BINARY_DIR}/venv
    COMMAND . ${CMAKE_BINARY_DIR}/venv/bin/activate && 
            ${CMAKE_BINARY_DIR}/venv/bin/pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
    COMMAND . ${CMAKE_BINARY_DIR}/venv/bin/activate && 
            ${CMAKE_BINARY_DIR}/venv/bin/python setup.py bdist_wheel
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Setting up environment and building wheel"
    DEPENDS CoreApp
    VERBATIM
)

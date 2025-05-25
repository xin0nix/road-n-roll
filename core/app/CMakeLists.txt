add_executable(CoreApp main.cpp)

if(WIN32)
    target_compile_options(CoreApp PRIVATE /EHsc)
endif()

target_link_libraries(CoreApp PRIVATE
    Router
    Server
    GameStore
    Boost::program_options
    Boost::log
    Boost::url
    Boost::beast
    Boost::json
    Boost::uuid
)

find_package(Python REQUIRED COMPONENTS Interpreter)

if(WIN32)
  set(PYCORE_BINARY "${CMAKE_SOURCE_DIR}/pycore/bin/CoreApp.exe")
else()
  set(PYCORE_BINARY "${CMAKE_SOURCE_DIR}/pycore/bin/CoreApp")
endif()

add_custom_command(
    OUTPUT ${PYCORE_BINARY}
    DEPENDS CoreApp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/pycore/bin
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CoreApp> ${PYCORE_BINARY}
    COMMENT "Copy CoreApp binary into pycore/bin"
)

add_custom_target(copy_coreapp ALL
    DEPENDS $<TARGET_FILE:CoreApp> ${PYCORE_BINARY}
)

if(WIN32)
  set(PYTHON_VENV_PYTHON "${CMAKE_BINARY_DIR}/venv/Scripts/python.exe")
else()
  set(PYTHON_VENV_PYTHON "${CMAKE_BINARY_DIR}/venv/bin/python")
endif()

add_custom_target(pycore_venv
    COMMAND ${Python_EXECUTABLE} -m venv ${CMAKE_BINARY_DIR}/venv
    COMMAND ${PYTHON_VENV_PYTHON} -m pip install --upgrade pip
    COMMAND ${PYTHON_VENV_PYTHON} -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
    COMMENT "Creating Python venv and installing requirements"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/venv
)


add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/core-api/setup.py
    COMMAND ${PYTHON_VENV_PYTHON} -m openapi_python_client generate
            --path ${CMAKE_SOURCE_DIR}/specs/openapi.yaml
            --output-path ${CMAKE_SOURCE_DIR}/core-api
            --overwrite --meta setup
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pycore_venv
    COMMENT "Generating OpenAPI client"
)

add_custom_target(generate_core_api
    DEPENDS ${CMAKE_SOURCE_DIR}/core-api/setup.py
)

add_custom_target(pycore_wheel
    COMMAND ${PYTHON_VENV_PYTHON} -m build --wheel --outdir ${CMAKE_BINARY_DIR}/dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS copy_coreapp pycore_venv CoreApp
    COMMENT "Building pycore wheel"
)

add_custom_target(core_api_wheel
    COMMAND ${PYTHON_VENV_PYTHON} -m build --wheel --outdir ${CMAKE_BINARY_DIR}/dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/core-api
    DEPENDS pycore_venv generate_core_api
    COMMENT "Building core-api wheel"
)

add_custom_target(build_all_wheels ALL
    DEPENDS pycore_wheel core_api_wheel
    COMMENT "Building all Python wheels"
)

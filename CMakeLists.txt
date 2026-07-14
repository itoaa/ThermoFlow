# ThermoFlow ESP-IDF CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_version.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE THERMOFLOW_VERSION_GEN_RESULT
    )
    if(NOT THERMOFLOW_VERSION_GEN_RESULT EQUAL 0)
        message(WARNING "Version generation failed; using fallback PROJECT_VER")
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/thermoflow_version.cmake OPTIONAL)
if(NOT THERMOFLOW_PROJECT_VER)
    set(THERMOFLOW_PROJECT_VER "0.0.0-dev")
endif()
set(PROJECT_VER ${THERMOFLOW_PROJECT_VER} CACHE STRING "ThermoFlow firmware version" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS components)

project(ThermoFlow)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
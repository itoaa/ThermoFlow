# ThermoFlow CMakeLists.txt
# ESP32-S3 Climate Monitoring and Control System

cmake_minimum_required(VERSION 3.16)

# Set target before project()
set(IDF_TARGET esp32s3)

# Include ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(ThermoFlow)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Component requirements
set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Security flags
add_compile_options(
    -Wall
    -Wextra
    -Werror=return-type
    -Werror=implicit-function-declaration
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

# Define project version
add_definitions(-DPROJECT_VERSION="1.0.0")
add_definitions(-DPROJECT_NAME="ThermoFlow")

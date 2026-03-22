# PlatformIO doesn't need src/CMakeLists.txt
# This file is kept for ESP-IDF compatibility only

idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "." "../include")
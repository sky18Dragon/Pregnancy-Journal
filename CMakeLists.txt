cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "2.0.0")
project(pregnancy_journal)

# PlatformIO asks CMake for the application target directly instead of the
# default ALL target. ESP-SR marks srmodels_bin as ALL, so make the relationship
# explicit for the isolated custom-partition voice profile.
if(TARGET srmodels_bin)
    add_dependencies(${CMAKE_PROJECT_NAME}.elf srmodels_bin)
endif()

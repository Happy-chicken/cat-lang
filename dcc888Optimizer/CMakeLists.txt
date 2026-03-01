cmake_minimum_required(VERSION 3.10)

# Set the project name
project(Dcc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Collect all source files
file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")


add_executable(Dcc ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${SOURCES})

target_compile_options(Dcc PRIVATE -g -o0 -fstandalone-debug)

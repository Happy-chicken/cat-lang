cmake_minimum_required(VERSION 3.10)
project(CatMLIR LANGUAGES CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
# set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
# file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")

add_subdirectory(src)
add_subdirectory(include)

add_executable(Cat ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_compile_options(Cat PRIVATE -g -o0 -fstandalone-debug)
target_link_libraries(Cat MLIRCatDialect MLIRGPUDialect)

cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CatLang)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

#Find the libgc
find_library(GC_LIB gc REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the compiler to clang++-14
set(CMAKE_CXX_COMPILER "/usr/bin/clang++-14")

# add include directories
include_directories(/usr/include/gc)

# Set the LLVM include directories and libraries
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# include directories
include_directories("${PROJECT_SOURCE_DIR}/include")

# Set the source files directory
set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")

# Collect all source files
file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")

# Add the executable
add_executable(CatLang main.cpp ${SOURCES})

#link the libgc
target_link_libraries(CatLang ${GC_LIB})

# Link the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS   
    Analysis
    Core
    ExecutionEngine
    InstCombine
    Object
    OrcJIT
    RuntimeDyld
    ScalarOpts
    Support
    native)
target_link_libraries(CatLang ${LLVM_LIBS})

target_compile_options(CatLang PRIVATE -fstandalone-debug)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

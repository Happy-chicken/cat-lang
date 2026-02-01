cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CatLang)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM package
find_package(LLVM 20.1.8 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# Find MLIR package
set(MLIR_CMAKE_DIR /usr/lib/llvm-20/lib/cmake/mlir)
# find_package(MLIR REQUIRED CONFIG)
message(STATUS "Found MLIR ${MLIR_PACKAGE_VERSION}")
message(STATUS "Found MLIRConfig.cmake in:  ${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
include(MLIRConfig)
include(AddLLVM)
include(AddMLIR)
include(TableGen)
#Find the libgc
# find_library(GC_LIB gc REQUIRED)

#include llvm and mlir
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})
# include_directories(/usr/include/gc)

add_subdirectory(compiler)
add_subdirectory(mlir)


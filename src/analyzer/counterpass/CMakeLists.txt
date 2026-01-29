cmake_minimum_required(VERSION 3.10)
project(CounterPass)

set(CMAKE_CXX_STANDARD 17)

set(LLVM_PATH /usr/lib/llvm-20)

include_directories(${LLVM_PATH}/include)

add_library(counterpass MODULE counterpass.cpp)

set_target_properties(counterpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
        )

target_link_libraries(counterpass ${LLVM_PATH}/lib/libLLVM-20.so)

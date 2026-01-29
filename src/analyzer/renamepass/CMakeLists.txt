cmake_minimum_required(VERSION 3.10)
project(MyPass)

set(CMAKE_CXX_STANDARD 17)

set(LLVM_PATH /usr/lib/llvm-20)

include_directories(${LLVM_PATH}/include)

add_library(mypass MODULE MyPass.cpp)

set_target_properties(mypass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
        )

target_link_libraries(mypass ${LLVM_PATH}/lib/libLLVM-20.so)

# clang-14 -S -emit-llvm -O0 -Xclang -disable-O0-optnone ./foo.c -o foo.ll
# opt-14 -S -load-pass-plugin=./build/libmypass.so -passes="mypass" foo.ll -o foo.ll

cmake_minimum_required(VERSION 3.10)

add_library(counterpass MODULE counterpass.cpp)

set_target_properties(counterpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)

# target_link_libraries(counterpass PRIVATE /usr/lib/llvm-20/lib/libLLVM-20.so)


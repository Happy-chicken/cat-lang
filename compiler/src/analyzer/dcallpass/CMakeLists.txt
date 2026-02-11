cmake_minimum_required(VERSION 3.10)

add_library(dcallpass MODULE dcallpass.cpp)

set_target_properties(dcallpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



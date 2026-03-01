cmake_minimum_required(VERSION 3.10)

add_library(dcepass MODULE dcepass.cpp)

set_target_properties(dcepass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



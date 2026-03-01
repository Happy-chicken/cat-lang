cmake_minimum_required(VERSION 3.10)

add_library(rivpass MODULE rivpass.cpp)

set_target_properties(rivpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



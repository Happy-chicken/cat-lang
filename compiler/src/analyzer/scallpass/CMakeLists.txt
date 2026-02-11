cmake_minimum_required(VERSION 3.10)

add_library(scallpass MODULE scallpass.cpp)

set_target_properties(scallpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



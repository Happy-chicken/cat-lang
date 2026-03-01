cmake_minimum_required(VERSION 3.10)

add_library(mergebbpass MODULE mergebbpass.cpp)

set_target_properties(mergebbpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



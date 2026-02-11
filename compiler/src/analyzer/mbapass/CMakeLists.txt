cmake_minimum_required(VERSION 3.10)

add_library(mbapass MODULE mbapass.cpp)

set_target_properties(mbapass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



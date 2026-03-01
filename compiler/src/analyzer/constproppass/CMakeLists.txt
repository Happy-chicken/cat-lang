cmake_minimum_required(VERSION 3.10)

add_library(constproppass MODULE constproppass.cpp)

set_target_properties(constproppass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



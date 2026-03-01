cmake_minimum_required(VERSION 3.10)

add_library(dupbbpass MODULE dupbbpass.cpp)
target_include_directories(dupbbpass PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../rivpass")
set_target_properties(dupbbpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

add_library(mypass MODULE MyPass.cpp)

set_target_properties(mypass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

# target_link_libraries(mypass PRIVATE LLVMCore LLVMSupport)


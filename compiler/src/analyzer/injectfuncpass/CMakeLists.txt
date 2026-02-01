add_library(injectfuncpass MODULE injectfuncpass.cpp)

set_target_properties(injectfuncpass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)



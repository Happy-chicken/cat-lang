
# include current include folder
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ast)


# include directories
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# set the test files directory
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

# Set the source files directory
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Collect all source files
file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/common/*.cpp"
                          "${SOURCE_DIR}/front-end/ast/*.cpp"
                          "${SOURCE_DIR}/front-end/scanner/*.cpp"
                          "${SOURCE_DIR}/front-end/parser/*.cpp"
                          "${SOURCE_DIR}/front-end/sematic/*.cpp"
                          "${SOURCE_DIR}/front-end/symbol/*.cpp"
                          "${SOURCE_DIR}/mid-end/codegen/*.cpp"
                          "${SOURCE_DIR}/mid-end/ir/*.cpp"
                          "${SOURCE_DIR}/mid-end/optimizer/*.cpp"
                          "${SOURCE_DIR}/runtime/*.cpp"
                          "${SOURCE_DIR}/back-end/*.cpp"
                          "${SOURCE_DIR}/vm/*.cpp")

# Add the executable
add_executable(CatLang ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${SOURCES})

# Link the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS   
    Analysis
    Core
    ExecutionEngine
    InstCombine
    Object
    OrcJIT
    RuntimeDyld
    ScalarOpts
    Support
    native)
target_link_libraries(CatLang PRIVATE ${LLVM_LIBS})

target_compile_options(CatLang PRIVATE -g -o0 -fstandalone-debug)

add_custom_target(test
    COMMAND CatLang build ${TEST_DIR}/testjit.cat
    DEPENDS CatLang
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set(PASS_DIR "${PROJECT_SOURCE_DIR}/build/compiler/src/analyzer")
add_custom_target(cpass
  COMMAND opt-20 -load-pass-plugin ${PASS_DIR}/counterpass/libcounterpass.so
                 -passes="print<counter>"
                 -S
                 ${TEST_DIR}/foo.ll
                 -o ${TEST_DIR}/counter.ll
  DEPENDS counterpass
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(ipass
  COMMAND opt-20 -load-pass-plugin ${PASS_DIR}/injectfuncpass/libinjectfuncpass.so
                 -passes="injectfuncpass"
                 -S
                 ${TEST_DIR}/foo.ll
                 -o ${TEST_DIR}/inject.ll
  DEPENDS injectfuncpass
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(scpass
  COMMAND opt-20 -load-pass-plugin ${PASS_DIR}/scallpass/libscallpass.so
                 -passes="print<scallpass>"
                 -S
                 ${TEST_DIR}/foo.ll
                 -o ${TEST_DIR}/scall.ll
  DEPENDS scallpass
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(dcpass
  COMMAND opt-20 -load-pass-plugin ${PASS_DIR}/dcallpass/libdcallpass.so
                 -passes="dcallpass"
                 -S
                 ${TEST_DIR}/foo.ll
                 -o ${TEST_DIR}/dcall.ll
  DEPENDS scallpass
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(mba
  COMMAND opt-20 -load-pass-plugin ${PASS_DIR}/mbapass/libmbapass.so
                 -passes="mbapass"
                 -S
                 -debugify-each
                 ${TEST_DIR}/foo.ll
                 -o ${TEST_DIR}/mba.ll
  DEPENDS mbapass
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_subdirectory(src/analyzer)

// #include "ASTPrinter.hpp"
#include "Cat.hpp"
#include <llvm-c-20/llvm-c/Types.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/CommandLine.h>
int main(int argc, char *argv[]) {
    Cat cat(argc, argv);
    enum OptLv { O0,
                 O1,
                 O2,
                 O3 };
    llvm::cl::opt<string> mode(
        llvm::cl::Positional,
        llvm::cl::desc("<mode>"),
        llvm::cl::Required
    );
    llvm::cl::opt<string> inputFile(llvm::cl::Positional, llvm::cl::desc("<input file>"), llvm::cl::Required);
    llvm::cl::opt<string> outputFile("o", llvm::cl::desc("Specify output file name"), llvm::cl::value_desc("filename"));
    llvm::cl::opt<OptLv> optLv(
        "Level",
        llvm::cl::desc("Choose optimization level"),
        llvm::cl::Prefix,
        llvm::cl::values(
            clEnumValN(OptLv::O0, "0", "No optimization"),
            clEnumValN(OptLv::O1, "1", "Trival optimization"),
            clEnumValN(OptLv::O2, "2", "Default optimization"),
            clEnumValN(OptLv::O3, "3", "Expensive optimization")
        ),
        llvm::cl::init(OptLv::O0)
    );
    llvm::cl::ParseCommandLineOptions(argc, argv, "Cat Language Compiler!\n");
    // = "/home/buyi/code/cat-lang/test/test.cat";
    cat.isUseJIT = true;
    llvm::OptimizationLevel optLevel = llvm::OptimizationLevel::O0;
    switch (optLv) {
        case OptLv::O0:
            optLevel = llvm::OptimizationLevel::O0;
            break;
        case OptLv::O1:
            optLevel = llvm::OptimizationLevel::O1;
            break;
        case OptLv::O2:
            optLevel = llvm::OptimizationLevel::O2;
            break;
        case OptLv::O3:
            optLevel = llvm::OptimizationLevel::O3;
            break;
    }
    if (mode == "run") {
        // cat.runFile(filePath);
    } else if (mode == "build") {
        cat.buildFile(inputFile, optLevel);
    }
    return 0;
}


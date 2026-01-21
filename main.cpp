// #include "ASTPrinter.hpp"
#include "Cat.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    std::string filePath;// = "/home/buyi/code/cat-lang/test/test.cat";
    std::string mode;
    Cat cat(argc, argv);
    cat.isUseJIT = true;
    llvm::OptimizationLevel optLevel = llvm::OptimizationLevel::O0;
    if (argc == 4) {
        mode = argv[1];
        filePath = argv[3];
        if (std::string(argv[2]) == "-O0") {
            optLevel = llvm::OptimizationLevel::O0;
        } else if (std::string(argv[2]) == "-O1") {
            optLevel = llvm::OptimizationLevel::O1;
        } else if (std::string(argv[2]) == "-O2") {
            optLevel = llvm::OptimizationLevel::O2;
        } else if (std::string(argv[2]) == "-O3") {
            optLevel = llvm::OptimizationLevel::O3;
        } else {
            std::cout << "Unknown optimization level: " << argv[2] << std::endl;
            return 1;
        }
        // if (mode == "run") cat.runFile(filePath);
        if (mode == "build") cat.buildFile(filePath, optLevel);
    } else if (argc == 3) {
        mode = argv[1];
        filePath = argv[2];
        if (mode == "run") {
            // cat.runFile(filePath);
        } else if (mode == "build") {
            cat.buildFile(filePath, llvm::OptimizationLevel::O0);
        }
    } else {
        std::cout << "\t[usage]: cat build <opt_level> <file.cat>\n\t\t cat run <file.cat>" << std::endl;
        std::cout << cat.logo << std::endl;
    }

    return 0;
}
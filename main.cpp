#include "./include/ASTPrinter.hpp"
#include "./include/Cat.hpp"
#include "./include/Logger.hpp"

int main(int argc, char *argv[]) {
    std::string filePath;// = "/home/buyi/code/cat-lang/test/test.cat";
    std::string mode;
    Cat cat(argc, argv);
    cat.isUseJIT = true;
    if (argc == 3) {
        mode = argv[1];
        filePath = argv[2];
        // ---------------------------------------------------------------------------
        Error::spnitList = Error::split(cat.readFile(filePath));
        // ---------------------------------------------------------------------------
        if (mode == "run") cat.runFile(filePath);
        if (mode == "build") cat.buildFile(filePath);
        // ---------------------------------------------------------------------------
    } else {
        std::cout << "\t[usage]: cat build <file.cat>\n\t\t cat run <file.cat>" << std::endl;
        std::cout << cat.logo << std::endl;
    }

    return 0;
}
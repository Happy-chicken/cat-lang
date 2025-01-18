#include "./include/ASTPrinter.hpp"
#include "./include/Cat.hpp"
#include "./include/Logger.hpp"

int main(int argc, char *argv[]) {
    std::string filePath;// = "./test.cat";
    Cat cat;
    cat.isJIT = true;
    if (argc > 1) {
        filePath = argv[1];
        // ---------------------------------------------------------------------------
        Error::spnitList = Error::split(cat.readFile(filePath));
        // ---------------------------------------------------------------------------
        cat.buildFile(filePath);
        // ---------------------------------------------------------------------------
    } else {
        std::cout << "\t[usage]: cat <file.cat>" << std::endl;
        std::cout << cat.logo << std::endl;
    }

    return 0;
}
#include "./include/ASTPrinter.hpp"
#include "./include/Cat.hpp"
#include "./include/Logger.hpp"

int main(int argc, char *argv[]) {
    std::string filePath;// = "./test.cat";

    if (argc > 1) {
        filePath = argv[1];
        // ---------------------------------------------------------------------------
        Error::spnitList = Error::split(Cat::readFile(filePath));
        // ---------------------------------------------------------------------------
        Cat::buildFile(filePath);
        // ---------------------------------------------------------------------------
    } else {
        std::cout << "\t[usage]: cat <file.cat>" << std::endl;
        std::cout << Cat::logo << std::endl;
    }

    return 0;
}
#pragma once

#include "IRGen.hpp"
#include "JIT.hpp"
#include "Logger.hpp"
#include "Parser.hpp"
#include "Resolver.hpp"
#include "Scanner.hpp"
#include "Stmt.hpp"
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
using std::string;

class Cat {
public:
    Cat(int argc_, char *argv_[]) : argc(argc_), argv(argv_) {}
    ~Cat() = default;
    void build(const std::string &program);
    void buildFile(std::string path);
    std::string readFile(std::string_view filename);

public:
    static std::string logo;
    bool isUseJIT = false;

private:
    int argc;
    char **argv;
};

#pragma once

#include "IRGen.hpp"
// #include "JIT.hpp"
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
    Cat() = default;
    ~Cat() = default;
    void build(const std::string &program);
    void buildFile(std::string path);
    std::string readFile(std::string_view filename);

public:
    static std::string logo;
    bool isJIT = false;
};

std::string Cat::logo{R"(
├─Welcome to Cat Programming Language!─┤
________________________________________
|      ___          ___                |       
|     /  /\        /  /\        ___    |       
|    /  /:/       /  /::\      /  /\   |       
|   /  /:/       /  /:/\:\    /  /:/   |       
|  /  /:/  ___  /  /:/~/::\  /  /:/    |       
| /__/:/  /  /\/__/:/ /:/\:\/  /::\    |       
| \  \:\ /  /:/\  \:\/:/__\/__/:/\:\   |       
|  \  \:\  /:/  \  \::/    \__\/  \:\  |        
|   \  \:\/:/    \  \:\         \  \:\ |        
|    \  \::/      \  \:\         \__\/ |       
|     \__\/        \__\/               |      
|______________________________________|
)"};

void Cat::build(const string &program) {
    // lexer analysis
    auto scanner = std::make_unique<Scanner>(program);
    auto tokens = scanner->scanTokens();
    // ---------------------------------------------------------------------------
    // syntax analysis
    auto parser = std::make_unique<Parser>(tokens);
    auto statements = parser->parse();
    if (Error::hadError) {
        Error::report();
        exit(EXIT_FAILURE);
    }
    // ---------------------------------------------------------------------------
    // semantic analysis
    auto resolver = std::make_shared<Resolver>();
    resolver->resolve(statements);
    if (Error::hadError) {
        Error::report();
        exit(EXIT_FAILURE);
    }
    // ---------------------------------------------------------------------------
    // intermediate representation generation
    auto ir_generator = std::make_shared<IRGenerator>();
    ir_generator->compileAndSave(statements);
    // ---------------------------------------------------------------------------
    if (Error::hadError) {
        Error::report();
    }
    // ---------------------------------------------------------------------------
    // JIT
    // if (isJIT) {

    //     typedef int (*AddFunctionType)(int, int);
    //     // JIT
    //     llvm::ExitOnError ExitOnErr;
    //     JIT catJIT{ir_generator};
    //     catJIT.createJIT();
    //     // TODO
    //     auto jit = catJIT.getJIT();

    //     auto module = catJIT.CreateModule();
    //     ExitOnErr(jit->addIRModule(std::move(module)));
    //     auto functionSymbol = ExitOnErr(jit->lookup("add"));
    //     AddFunctionType add = (AddFunctionType) functionSymbol.getAddress();

    //     // Use the "Add()" function
    //     int result = add(12, 34);
    //     std::cout << "\n-----------------\n";
    //     std::cout << "Add(12, 34) = " << result << std::endl;
    // }
}

std::string Cat::readFile(std::string_view filename) {
    std::ifstream file{filename.data(), std::ios::ate};
    if (!file) {
        std::cerr << "Failed to open file " << filename.data() << '\n';
        std::exit(74);// I/O error
    }
    std::string buffer;
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), buffer.size());
    file.close();

    return buffer;
}


void Cat::buildFile(string path) {
    std::string source = "{" + readFile(path) + "\n}";
    build(source);
}

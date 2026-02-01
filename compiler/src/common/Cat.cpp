#include "Cat.hpp"
#include "CodeGen.hpp"
#include "CodeGenCtx.hpp"
#include "Diagnostics.hpp"
#include "Jit.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"
#include "PassDriver.hpp"
#include "Scanner.hpp"
#include "SemanticCtx.hpp"
#include "SymbolTable.hpp"
#include "catlib.hpp"
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <llvm-20/llvm/IR/Verifier.h>
#include <llvm-20/llvm/Passes/OptimizationLevel.h>
#include <utility>
std::string Cat::logo{R"(
├─Welcome to Cat Programming Language!─┤
________________________________________
|               |\__/,|   (`\          |
|             _.|o o  |_   ) )         |
|-------------(((---(((----------------|        
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

// void Cat::run(const std::string &program) {
//     // lexer analysis
//     auto scanner = std::make_unique<Scanner>(program);
//     auto tokens = scanner->scanTokens();
//     // ---------------------------------------------------------------------------
//     // syntax analysis
//     auto parser = std::make_unique<Parser>(tokens);
//     auto statements = parser->parse();
//     if (Error::hadError) {
//         Error::report();
//         exit(EXIT_FAILURE);
//     }
//     // ---------------------------------------------------------------------------
//     // semantic analysis
//     // auto resolver = std::make_shared<Resolver>();
//     // resolver->resolve(statements);
//     // if (Error::hadError) {
//     //     Error::report();
//     //     exit(EXIT_FAILURE);
//     // }
//     // ---------------------------------------------------------------------------
//     // vm execution
//     auto vm = std::make_shared<CatStackVM>();

//     auto res = vm->run(statements);
//     vm->dump();
//     std::cout << "Result: " << AS_INT(res) << '\n';
// }

void Cat::build(const string &program, llvm::OptimizationLevel optLevel) {
    // define diagnostics
    auto diagnostics = Diagnostics();
    cat::setGlobalDiagnostics(&diagnostics);
    try {
        // ---------------------------------------------------------------------------

        // lexer analysis
        auto scanner = Scanner(program);
        // ---------------------------------------------------------------------------

        // syntax analysis
        auto parser = Parser(scanner);
        auto root = parser.parse();
        root->print(std::cout);
        // ---------------------------------------------------------------------------

        // semantic analysis
        // 知道变量类型，作用域，函数调用，重定义，未定义等行为
        auto passDriver = PassDriver(*root);
        auto symbolTable = SymbolTable();
        auto semanticCtx = SemanticCtx(symbolTable, diagnostics);
        Catime::declareBuiltins(semanticCtx);
        passDriver.runSemanticPass(semanticCtx);
        passDriver.runControlFlowPass(semanticCtx);
        // semanticCtx.dumpSymbolTable(std::cout);
        // semanticCtx.dumpFuncFrames(std::cout);
        // ---------------------------------------------------------------------------

        // intermediate representation generation using LLVM
        CodeGenCtx codeGenCtx("Cat_Module");
        CodeGen codeGen(codeGenCtx);
        Catime::genBuiltins(semanticCtx, codeGen);
        codeGen.compile(root);
        // ---------------------------------------------------------------------------
        // optimize the generated IR
        Optimizier optimizer;
        optimizer.optimize(codeGenCtx.getModule(), optLevel);
        if (llvm::verifyModule(codeGenCtx.getModule(), &llvm::errs())) {
            std::cerr << "Error: Generated LLVM IR is invalid.\n";
            exit(1);
        }
        optimizer.save(codeGenCtx.getModule());
        // ---------------------------------------------------------------------------

        // JIT
        JIT catJit{codeGen};
        llvm::InitLLVM X(argc, argv);
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
        uptr<llvm::Module> M = catJit.loadModule();
        auto llvmctx = codeGen.getContext().releaseLLVMContext();

        llvm::ExitOnError ExitOnErr(std::string(argv[0]) + ": ");
        ExitOnErr(catJit.run(std::move(M), std::move(llvmctx), argc, argv));

    } catch (const std::runtime_error &e) {
        diagnostics.printAll();
        std::cerr << "Build failed: " << e.what() << std::endl;
        return;
    }
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


void Cat::buildFile(string path, llvm::OptimizationLevel optLevel) {
    std::string source = readFile(path);
    build(source, optLevel);
}

// void Cat::runFile(string path) {
//     std::string source = readFile(path);
//     run(source);
// }

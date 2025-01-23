#include "../include/Cat.hpp"

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
        exit(EXIT_FAILURE);
    }
    // ---------------------------------------------------------------------------
    // JIT
    if (isUseJIT) {
        JIT catJit(ir_generator);
        llvm::InitLLVM X(argc, argv);
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        std::unique_ptr<llvm::Module> M = catJit.loadModule();

        llvm::ExitOnError ExitOnErr(std::string(argv[0]) + ": ");
        ExitOnErr(catJit.run(std::move(M), ir_generator->getContext(), argc, argv));
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


void Cat::buildFile(string path) {
    std::string source = "{" + readFile(path) + "\n}";
    build(source);
}

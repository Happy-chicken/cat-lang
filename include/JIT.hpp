#include "./IRGen.hpp"
#include "llvm-14/llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm-14/llvm/IRReader/IRReader.h"
#include "llvm-14/llvm/Support/CommandLine.h"
#include "llvm-14/llvm/Support/InitLLVM.h"
#include "llvm-14/llvm/Support/TargetSelect.h"
#include <llvm-14/llvm/IR/Module.h>
#include <llvm-c-14/llvm-c/Types.h>
#include <memory>
class JIT {
private:
    std::shared_ptr<IRGenerator> ir_generator;

public:
    // static llvm::cl::opt<std::string> InputFile(llvm::cl::Positional, llvm::cl::Required, llvm::cl::desc("<input-file >"));
    JIT(std::shared_ptr<IRGenerator> ir_generator_) : ir_generator(ir_generator_) {}
    std::unique_ptr<llvm::Module> loadModule();
    llvm::Error run(std::unique_ptr<llvm::Module> module, std::unique_ptr<llvm::LLVMContext> ctx, int argc, char *argv[]);
};

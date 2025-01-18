#include "../include/JIT.hpp"
#include <llvm-14/llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

void JIT::createJIT() {
    // Create the JIT
    jit = ExitOnErr(llvm::orc::LLJITBuilder().create());
    // safeModule = CreateModule();
}

llvm::orc::ThreadSafeModule JIT::CreateModule() {
    auto context = irgenerator->getContext();
    auto module = irgenerator->getModule();
    return llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
}

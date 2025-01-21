#include "./IRGen.hpp"
#include <iostream>
#include <llvm-14/llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm-14/llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm-14/llvm/Support/Error.h>
#include <llvm-14/llvm/Support/InitLLVM.h>
#include <llvm-14/llvm/Support/TargetSelect.h>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <memory>
#include <utility>
#include <vector>


class JIT {
public:
    JIT(std::shared_ptr<IRGenerator> irgenerator_) : irgenerator{irgenerator_} {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
    }

    void createJIT();
    llvm::orc::ThreadSafeModule CreateModule();

    std::unique_ptr<llvm::orc::LLJIT> getJIT() {
        return std::move(jit);
    }

    // llvm::orc::ThreadSafeModule getSafeModule() {
    //     return std::move(safeModule);
    // }


private:
    std::unique_ptr<llvm::orc::LLJIT> jit;
    std::shared_ptr<IRGenerator> irgenerator;
    llvm::ExitOnError ExitOnErr;
    llvm::orc::ThreadSafeModule safeModule;
};

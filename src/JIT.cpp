#include "../include/JIT.hpp"
#include <cstdlib>
#include <llvm-14/llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm-14/llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm-14/llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm-14/llvm/IR/DataLayout.h>
#include <llvm-14/llvm/IR/LLVMContext.h>
#include <llvm-14/llvm/IR/Module.h>
#include <llvm-14/llvm/Support/CommandLine.h>
#include <llvm-14/llvm/Support/Error.h>
#include <llvm-14/llvm/Support/InitLLVM.h>
#include <llvm-14/llvm/Support/SourceMgr.h>
#include <llvm-14/llvm/Support/TargetSelect.h>
#include <memory>
#include <utility>

std::unique_ptr<llvm::Module> JIT::loadModule() {
    llvm::SMDiagnostic err;
    auto module = ir_generator->getModule();
    if (!module) {
        err.print("main", llvm::errs());
        exit(EXIT_FAILURE);
    }
    return std::move(module);
}

llvm::Error JIT::run(std::unique_ptr<llvm::Module> module, std::unique_ptr<llvm::LLVMContext> ctx, int argc, char *argv[]) {
    // auto JIT = llvm::orc::LLJITBuilder().create();// create LLJIT, llvm<expected> is a pointer
    auto JIT = CatJIT::Create();
    if (!JIT) {
        return JIT.takeError();
    }
    // add module to the jit
    if (auto err = (*JIT)->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        return err;
    }
    // add dynamic library search resolve symbols from the host process
    // add the current process's dynamic library search generator to the JIT's main dylib
    // const llvm::DataLayout &DL = (*JIT)->getDataLayout();
    // auto DLSG = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix());
    // if (!DLSG) {
    //     return DLSG.takeError();
    // }
    // (*JIT)->getMainJITDylib().addGenerator(std::move(*DLSG));

    // search main symbol
    auto mainSys = (*JIT)->lookup("main");
    if (!mainSys) {
        return mainSys.takeError();
    }
    auto *main = (int (*)(int, char **)) mainSys->getAddress();

    // call main function in IR
    (void) main(argc, argv);
    return llvm::Error::success();
}

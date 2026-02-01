#include "Jit.hpp"
#include <cstdlib>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
#include <utility>
extern "C" void cat_print(int x);

uptr<llvm::Module> JIT::loadModule() {
    llvm::SMDiagnostic err;
    auto module = ir_generator.getContext().releaseModule();
    if (!module) {
        err.print("main", llvm::errs());
        exit(EXIT_FAILURE);
    }
    return std::move(module);
}

llvm::Error JIT::run(uptr<llvm::Module> module, uptr<llvm::LLVMContext> ctx, int argc, char *argv[]) {
    auto JIT = CatJIT::Create();
    if (!JIT) {
        return JIT.takeError();
    }

    // 手动添加符号映射 - 在添加模块之前
    auto &MainJD = (*JIT)->getMainJITDylib();
    llvm::orc::SymbolMap Symbols;
    llvm::orc::MangleAndInterner Mangle((*JIT)->getMainJITDylib().getExecutionSession(), (*JIT)->getDataLayout());

    // 将 cat_print 函数地址添加到符号表
    Symbols[Mangle("cat_print")] = {
        llvm::orc::ExecutorAddr::fromPtr(reinterpret_cast<void *>(&cat_print)),
        llvm::JITSymbolFlags::Callable | llvm::JITSymbolFlags::Exported
    };

    llvm::cantFail(MainJD.define(llvm::orc::absoluteSymbols(Symbols)));

    // add module to the jit
    if (auto err = (*JIT)->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        return err;
    }

    // add dynamic library search resolve symbols from the host process
    const llvm::DataLayout &DL = (*JIT)->getDataLayout();
    auto DLSG = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix());
    if (!DLSG) {
        return DLSG.takeError();
    }
    (*JIT)->getMainJITDylib().addGenerator(std::move(*DLSG));

    // search main symbol
    auto mainSys = (*JIT)->lookup("main");
    if (!mainSys) {
        return mainSys.takeError();
    }
    auto main = mainSys->getAddress().toPtr<int (*)(int, char **)>();

    // call main function in IR
    (void) main(argc, argv);
    return llvm::Error::success();
}

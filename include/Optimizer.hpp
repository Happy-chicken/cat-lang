#pragma once
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
class Optimizier {
public:
    Optimizier();

    void optimize(llvm::Module &module, llvm::OptimizationLevel);
    void save(llvm::Module &module) {
        std::error_code errorCode;
        llvm::raw_fd_ostream outLL("./opt.ll", errorCode);
        module.print(outLL, nullptr);
    }

private:
    llvm::LoopAnalysisManager loopAM;
    llvm::FunctionAnalysisManager functionAM;
    llvm::CGSCCAnalysisManager cgsccAM;
    llvm::ModuleAnalysisManager moduleAM;
    llvm::PassBuilder passBuilder;
};

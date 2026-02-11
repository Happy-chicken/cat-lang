#pragma once

#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/Module.h>
#include <llvm-20/llvm/IR/PassManager.h>
namespace llvm {
  class DynamicCallPass : public llvm::PassInfoMixin<DynamicCallPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    bool runOnModule(Module &M);

    static bool isRequired() { return true; }
  };
}// namespace llvm

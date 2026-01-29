#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

namespace llvm {

    class MyPass : public PassInfoMixin<MyPass> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };

}// namespace llvm

// Register the pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MyPass", "v0.1",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if (Name == "mypass") {
                        FPM.addPass(llvm::MyPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}

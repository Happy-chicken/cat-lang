#include "dcepass.h"
#include <llvm-20/llvm/Passes/PassBuilder.h>
#include <llvm-20/llvm/Passes/PassPlugin.h>

namespace llvm {
  PreservedAnalyses DcePass::run(Function &F, FunctionAnalysisManager &FAM) {}

  bool DcePass::runOnFunction(Function &F) {}

  extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
  llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "DcePass", "v0.1",
        [](llvm::PassBuilder &PB) {
          // register for this counterpass
          PB.registerPipelineParsingCallback(
              [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                if (Name == "dce") {
                  FPM.addPass(DcePass());
                  return true;
                }
                return false;
              }
          );
        }
    };
  }

}// namespace llvm

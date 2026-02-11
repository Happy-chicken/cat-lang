#include "scallpass.h"
#include "llvm-20/llvm/Passes/PassBuilder.h"
#include "llvm-20/llvm/Passes/PassPlugin.h"
#include <llvm-20/llvm/IR/InstrTypes.h>
#include <utility>
using namespace llvm;
AnalysisKey StaticCallPass::Key;

static void printStaticCallResult(raw_ostream &out, StaticCallPass::Result SCallMap) {
  out << "================================================="
      << "\n";
  out << "LLVM: Static Call Counter results\n";
  out << "=================================================\n";
  const char *str1 = "NAME:";
  const char *str2 = "#DIRECT CALLS USED";
  out << format("%-20s %-10s\n", str1, str2);
  out << "-------------------------------------------------"
      << "\n";
  for (auto &Call: SCallMap) {
    out << format("%-20s %-10lu\n", Call.first->getName().data(), Call.second);
  }
  out << "-------------------------------------------------"

      << "\n\n";
}

StaticCallPass::Result
StaticCallPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return runOnModule(M);
}

StaticCallPass::Result StaticCallPass::runOnModule(Module &M) {
  StaticCallPass::Result Res;

  for (auto &fn: M) {
    for (auto &bb: fn) {
      for (auto &inst: bb) {
        auto *CB = dyn_cast<CallBase>(&inst);
        if (CB == nullptr) {
          continue;
        }
        // we need caller must directly call a function
        auto DirectInvoc = CB->getCalledFunction();
        if (DirectInvoc == nullptr) {
          continue;
        }
        auto CallCount = Res.find(DirectInvoc);
        if (CallCount == Res.end()) {
          CallCount = Res.insert(std::make_pair(DirectInvoc, 0)).first;
        }
        ++CallCount->second;
      }
    }
  }

  return Res;
}

PreservedAnalyses PrinterPass::run(Module &M, ModuleAnalysisManager &MAM) {
  auto &SCallMap = MAM.getResult<StaticCallPass>(M);
  printStaticCallResult(os, SCallMap);
  return PreservedAnalyses::all();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "SCallCounter", "v0.1",
      [](llvm::PassBuilder &PB) {
        // register for this counterpass
        PB.registerPipelineParsingCallback(
            [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
               llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
              if (Name == "print<scallpass>") {
                MPM.addPass(PrinterPass(llvm::errs()));
                return true;
              }
              return false;
            }
        );
        // PB.registerVectorizerStartEPCallback(
        //     [](llvm::FunctionPassManager &PM,
        //        llvm::OptimizationLevel Level) {
        //         PM.addPass(PrinterPass(llvm::errs()));
        //     }
        // );
        // register for OpCodePrinter can request for this counterpass
        PB.registerAnalysisRegistrationCallback(
            [](ModuleAnalysisManager &MAM) {
              MAM.registerPass([&] { return StaticCallPass(); });
            }
        );
      }
  };
}

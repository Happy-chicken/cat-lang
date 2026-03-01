#include "rivpass.h"
#include <llvm-20/llvm/ADT/SmallPtrSet.h>
#include <llvm-20/llvm/IR/Argument.h>
#include <llvm-20/llvm/IR/Dominators.h>
#include <llvm-20/llvm/Passes/PassBuilder.h>
#include <llvm-20/llvm/Passes/PassPlugin.h>
#include <queue>
namespace llvm {
  using NodeTy = DomTreeNodeBase<BasicBlock> *;
  static void printRIVResult(raw_ostream &Out, const RivPass::Result &RIVMap) {
    Out << "=================================================\n";
    Out << " RIV analysis results\n";
    Out << "=================================================\n";

    const char *Str1 = "BB id";
    const char *Str2 = "Reachable Integer Values";
    Out << format("%-10s %-30s\n", Str1, Str2);
    Out << "-------------------------------------------------\n";

    const char *EmptyStr = "";

    for (auto const &KV: RIVMap) {
      std::string DummyStr;
      raw_string_ostream BBIdStream(DummyStr);
      KV.first->printAsOperand(BBIdStream, false);
      Out << format("BB %-12s %-30s\n", BBIdStream.str().c_str(), EmptyStr);
      for (auto const *IntegerValue: KV.second) {
        std::string DummyStr;
        raw_string_ostream InstrStr(DummyStr);
        IntegerValue->print(InstrStr);
        Out << format("%-12s %-30s\n", EmptyStr, InstrStr.str().c_str());
      }
    }

    Out << "\n\n";
  }

  RivPass::Result RivPass::run(Function &F, FunctionAnalysisManager &FAM) {
    DominatorTree *DT = &FAM.getResult<DominatorTreeAnalysis>(F);
    Result res = buildRIV(F, DT->getRootNode());

    return res;
  }

  RivPass::Result RivPass::buildRIV(Function &F, DomTreeNodeBase<BasicBlock> *CFGRoot) {
    Result resultMap;
    // double end queue to traverse BasicBlock in F
    std::deque<NodeTy> BBsToProcess;
    BBsToProcess.push_back(CFGRoot);

    Result definedValuesMap;
    // 1. for each bb, compute set of integer value defined in bb
    for (auto &bb: F) {
      auto &values = definedValuesMap[&bb];
      for (auto &inst: bb) {
        if (inst.getType()->isIntegerTy()) {
          values.insert(&inst);
        }
      }
    }
    // 2. compute rivs for entry bb, this will include global variable and args
    auto &entryBBValues = definedValuesMap[&F.getEntryBlock()];
    for (auto &glob: F.getParent()->globals()) {
      if (glob.getValueType()->isIntegerTy()) {
        entryBBValues.insert(&glob);
      }
    }
    for (Argument &arg: F.args()) {
      if (arg.getType()->isIntegerTy()) {
        entryBBValues.insert(&arg);
      }
    }
    // 3. traverse CFG for every bb in F and calculate its rivs
    // dfs of DominatorTree
    while (!BBsToProcess.empty()) {
      auto *parent = BBsToProcess.back();
      BBsToProcess.pop_back();

      // get values defined in parent
      auto &parentDefs = definedValuesMap[parent->getBlock()];
      // get riv set for parent
      // rivmap is updated each iteration, so we need a copy
      SmallPtrSet<Value *, 8> parentRIVs = resultMap[parent->getBlock()];

      // loop over all blocks parent dominates and update riv set
      for (NodeTy child: *parent) {
        BBsToProcess.push_back(child);
        auto childBB = child->getBlock();
        // RIV(child) = RIV(parent) ∪ Defs(parent)
        resultMap[childBB].insert(parentDefs.begin(), parentDefs.end());
        resultMap[childBB].insert(parentRIVs.begin(), parentRIVs.end());
      }
    }
    return resultMap;
  }

  PreservedAnalyses PrinterPass::run(Function &F, FunctionAnalysisManager &FAM) {
    auto rivMap = FAM.getResult<RivPass>(F);
    printRIVResult(os, rivMap);
    return PreservedAnalyses::all();
  }

  extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
  llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "RivPass", "v0.1",
        [](llvm::PassBuilder &PB) {
          // register for this counterpass
          PB.registerPipelineParsingCallback(
              [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                if (Name == "print<rivpass>") {
                  FPM.addPass(PrinterPass(llvm::errs()));
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
              [](FunctionAnalysisManager &FAM) {
                FAM.registerPass([&] { return RivPass(); });
              }
          );
        }
    };
  }
}// namespace llvm

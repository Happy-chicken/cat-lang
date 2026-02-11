#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/PassManager.h>
namespace llvm {
  struct MbaPass : public PassInfoMixin<MbaPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &);
    bool runOnBasicBlock(BasicBlock &bb);
    static bool isRequired() { return true; }
  };
}// namespace llvm


#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/PassManager.h>
namespace llvm {
  class DcePass : public llvm::PassInfoMixin<DcePass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
    bool runOnFunction(Function &F);

    static bool isRequired() { return true; }
  };
}// namespace llvm

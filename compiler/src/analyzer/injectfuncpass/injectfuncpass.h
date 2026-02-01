#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>

namespace llvm {
    class InjectFuncPass : public PassInfoMixin<InjectFuncPass> {
    public:
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
        bool runOnModule(Module &M);

        static bool isRequired() { return true; }
    };
}// namespace llvm

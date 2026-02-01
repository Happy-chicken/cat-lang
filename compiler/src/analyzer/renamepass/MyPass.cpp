#include "./MyPass.h"
namespace llvm {
    static int f_number = 0;
    PreservedAnalyses MyPass::run(Function &F, FunctionAnalysisManager &AM) {
        // rename function to f_number
        F.setName("function" + std::to_string(f_number++));
        return PreservedAnalyses::all();
    }
}// namespace llvm

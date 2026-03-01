#include <llvm-20/llvm/ADT/MapVector.h>
#include <llvm-20/llvm/ADT/SmallPtrSet.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/PassManager.h>
#include <llvm-20/llvm/Support/GenericDomTree.h>
namespace llvm {
  class RivPass : public AnalysisInfoMixin<RivPass> {
public:
    using Result = MapVector<BasicBlock const *, SmallPtrSet<Value *, 8>>;
    Result run(Function &F, FunctionAnalysisManager &FAM);
    Result buildRIV(Function &F, DomTreeNodeBase<BasicBlock> *CFGRoot);

private:
    static AnalysisKey Key;
    friend struct AnalysisInfoMixin<RivPass>;
  };

  class PrinterPass : public PassInfoMixin<PrinterPass> {
public:
    explicit PrinterPass(raw_ostream &out) : os(out) {}
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
    static bool isRequired() { return true; }
    static StringRef name() { return "PrinterPass"; }

private:
    raw_ostream &os;
  };
}// namespace llvm

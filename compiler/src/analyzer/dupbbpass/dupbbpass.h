#include <llvm-20/llvm/ADT/MapVector.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/Module.h>
#include <llvm-20/llvm/IR/PassManager.h>
#include <llvm-20/llvm/IR/Value.h>
#include <llvm-20/llvm/Support/RandomNumberGenerator.h>
#include <map>
#include <memory>
#include <tuple>
#include <vector>
namespace llvm {
  class RandomNumberGenerator;
  class DuplicateBBPass : public PassInfoMixin<DuplicateBBPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
    using RIVMap = MapVector<BasicBlock const *, SmallPtrSet<Value *, 8>>;
    // map bb to a reachable value in BasicBlock
    using BBToSingleRIVMap = std::vector<std::tuple<BasicBlock *, Value *>>;
    // map a value before duplicate to a phi node
    using ValueToPhiMap = std::map<Value *, Value *>;
    BBToSingleRIVMap findBBsToDuplicate(Function &F, const RIVMap &RIVResult);

    // clone input BasicBlock
    // inject if then else
    // duplicate bb
    // add phi node
    void cloneBB(BasicBlock &bb, Value *contextValue, ValueToPhiMap &reMapper);
    static bool isRequired() { return true; }

private:
    unsigned duplicateBBCount = 0;
    std::unique_ptr<llvm::RandomNumberGenerator> pRNG;
  };
}// namespace llvm

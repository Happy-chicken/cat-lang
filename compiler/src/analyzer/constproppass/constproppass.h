#include <cstdint>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/Instruction.h>
#include <llvm-20/llvm/IR/PassManager.h>
#include <llvm-20/llvm/IR/Value.h>
#include <map>
enum class ValueType {
  UNKOWN,
  CONST,
  NAC
};

struct ValueState {
  ValueType kind;
  int64_t constVal;

  ValueState(ValueType kind, int constVal = 0) : kind(kind), constVal(constVal) {}

  static ValueState makeTop() {
    return {ValueType::UNKOWN};
  }

  static ValueState makeConstant(int val) {
    return {ValueType::CONST, val};
  }

  static ValueState makeBottom() {
    return {ValueType::NAC};
  }

  bool isTop() const { return kind == ValueType::UNKOWN; }
  bool isConst() const { return kind == ValueType::CONST; }
  bool isBottom() const { return kind == ValueType::NAC; }

  ValueState meet(const ValueState &other) const {
    if (isBottom() || other.isBottom()) {
      return makeBottom();
    }
    if (isTop()) return makeTop();
    if (other.isTop()) return *this;
    if (constVal == other.constVal) {
      return *this;
    }
    return makeBottom();
  }
};
namespace llvm {
  class ConstantPropagationPass : public llvm::PassInfoMixin<ConstantPropagationPass> {
public:
    using StateMap = std::map<Value *, ValueState>;

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
    bool runOnFunction(Function &F);
    static bool isRequired() { return true; }

private:
    bool stateChanged(const ValueState &oldState, const ValueState &newState) const;
    ValueState evaluate(Instruction &I, StateMap &state);
    ValueState getState(Value *val, StateMap &state);
    void applyResults(Function &F, StateMap &state);
  };
}// namespace llvm

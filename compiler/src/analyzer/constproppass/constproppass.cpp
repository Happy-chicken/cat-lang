#include "constproppass.h"
#include <llvm-20/llvm/ADT/SetVector.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/Constants.h>
#include <llvm-20/llvm/IR/Instruction.h>
#include <llvm-20/llvm/Passes/PassBuilder.h>
#include <llvm-20/llvm/Passes/PassPlugin.h>
#include <map>
#include <queue>
#include <vector>
namespace llvm {
  PreservedAnalyses ConstantPropagationPass::run(Function &F, FunctionAnalysisManager &FAM) {
    bool changed = runOnFunction(F);
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool ConstantPropagationPass::runOnFunction(Function &F) {
    StateMap state;
    bool changed = false;
    // init all variable state
    for (auto &BB: F) {
      for (auto &I: BB) {
        state[&I] = ValueState::makeTop();
      }
    }
    for (auto &arg: F.args()) {
      state[&arg] = ValueState::makeBottom();
    }

    // build dep set
    std::map<Value *, std::vector<Instruction *>> users;
    for (auto &BB: F) {
      for (auto &I: BB) {
        for (auto *op: I.operand_values()) {
          users[op].push_back(&I);
        }
      }
    }
    // init worklist
    SetVector<Instruction *> worklist;
    for (auto &BB: F) {
      for (auto &I: BB) {
        worklist.insert(&I);
      }
    }

    // fix point algorithm
    while (!worklist.empty()) {
      Instruction *curInst = worklist.pop_back_val();

      ValueState newState = evaluate(*curInst, state);
      if (!stateChanged(state[curInst], newState)) {
        continue;
      }
      state[curInst] = newState;
      changed = true;
      for (Instruction *user: users[curInst]) {
        worklist.insert(user);
      }
    }

    // lfp
    applyResults(F, state);
    return changed;
  }

  bool ConstantPropagationPass::stateChanged(const ValueState &oldState, const ValueState &newState) const {
    if (oldState.isBottom()) return false;
    if (newState.isBottom()) return true;                   // Top/Const → Bottom
    if (oldState.isTop() && newState.isConst()) return true;// Top → Const
    if (oldState.isConst() && newState.isConst())
      return oldState.constVal != newState.constVal;
    return false;
  }

  ValueState ConstantPropagationPass::evaluate(Instruction &I, ConstantPropagationPass::StateMap &state) {
    // PHI 节点：对所有来源做 meet
    if (auto *phi = dyn_cast<PHINode>(&I)) {
      ValueState result = ValueState::makeTop();
      for (auto &incoming: phi->incoming_values()) {
        result = result.meet(getState(incoming, state));
      }
      return result;
    }

    // 二元运算
    if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
      ValueState lhs = getState(binOp->getOperand(0), state);
      ValueState rhs = getState(binOp->getOperand(1), state);

      if (lhs.isBottom() || rhs.isBottom()) return ValueState::makeBottom();
      if (lhs.isTop() || rhs.isTop()) return ValueState::makeTop();

      int64_t result;
      switch (binOp->getOpcode()) {
        case Instruction::Add:
          result = lhs.constVal + rhs.constVal;
          break;
        case Instruction::Mul:
          result = lhs.constVal * rhs.constVal;
          break;
        case Instruction::Sub:
          result = lhs.constVal - rhs.constVal;
          break;
        default:
          return ValueState::makeBottom();
      }
      return ValueState::makeConstant(result);
    }

    return ValueState::makeBottom();
  }

  ValueState ConstantPropagationPass::getState(Value *val, ConstantPropagationPass::StateMap &state) {
    if (auto *C = dyn_cast<ConstantInt>(val)) {
      return ValueState::makeConstant(C->getSExtValue());
    }
    if (state.count(val)) {
      return state[val];
    }
    return ValueState::makeBottom();
  }

  void ConstantPropagationPass::applyResults(Function &F, ConstantPropagationPass::StateMap &state) {
    for (auto &BB: F) {
      for (auto &I: BB) {
        if (state[&I].isConst()) {
          auto *C = ConstantInt::get(I.getType(), state[&I].constVal);
          I.replaceAllUsesWith(C);
        }
      }
    }
  }

}// namespace llvm

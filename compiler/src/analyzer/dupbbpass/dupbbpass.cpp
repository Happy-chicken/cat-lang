#include "dupbbpass.h"
#include "../rivpass/rivpass.h"
#include <cassert>
#include <cstddef>
#include <iterator>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/Instruction.h>
#include <llvm-20/llvm/IR/Value.h>
#include <llvm-20/llvm/Passes/PassBuilder.h>
#include <llvm-20/llvm/Passes/PassPlugin.h>
#include <llvm-20/llvm/Support/Debug.h>
#include <llvm-20/llvm/Support/raw_ostream.h>
#include <llvm-20/llvm/Transforms/Utils/BasicBlockUtils.h>
#include <random>
#include <string>
//  DESCRIPTION:
//    For the input function F, DuplicateBB first creates a list of BasicBlocks
//    that are suitable for cloning and then clones them. A BasicBlock BB is
//    suitable for cloning iff its set of RIVs (Reachable Integer Values) is
//    non-empty.
//
//    If BB is suitable for cloning, in total four new basic blocks are created
//    that replace BB:
//      * lt-clone-1 and lt-clone-2 are clones of BB
//      * lt-if-then-else contains an `if-then-else` statement that's used to
//        decide which out of the two clones to branch two
//      * lt-tail contains PHI nodes and is used to merge lt-clone-1 and
//        lt-clone-2
//    This if-then-else basic blocks contains an IR equivalent of the following
//    pseudo code:
//      if (var == 0)
//        goto BB-if-then
//      else
//        goto BB-else
//    `var` is a randomly chosen variable from the RIV set for BB. If `var`
//    happens to be a GlobalValue (i.e. global variable), BB won't be
//    duplicated. That's because global variables are often constants, and
//    constant values lead to trivial `if` conditions (e.g. if ( 0 == 0 )).
//
//    All newly created basic blocks are suffixed with the original basic
//    block's numeric ID.
//
//  ALGORITHM:
//    --------------------------------------------------------------------------
//    The following CFG graph represents function 'F' before and after applying
//    DuplicateBB. Assume that:
//      * F takes no arguments so [ entry ] is not duplicated
//      * [ BB1 ] and [ BB2 ] are suitable for cloning
//    --------------------------------------------------------------------------
//    F - BEFORE     (equivalence)          F - AFTER
//    --------------------------------------------------------------------------
//    [ entry ]                              [ entry ]                         |
//        |                                      |                             |
//        v            _________                 v                             e
//        |           |                  [ if-then-else-1 ]                    x
//        |           |                         / \                            e
//        |           |                       /     \                          c
//        |           |          [ lt-clone-1-1 ] [ lt-clone-1-2 ]             u
//     [ BB1 ]       <                        \     /                          t
//        |           |                         \ /                            i
//        |           |                          v                             o
//        |           |                    [ lt-tail-1 ]                       n
//        |           |_________                 |                             |
//        v            _________                 v                             d
//        |           |                 [ lt-if-then-else-2 ]                  i
//        |           |                         / \                            r
//        |           |                       /     \                          e
//        |           |          [ lt-clone-2-1 ] [ lt-clone-2-2 ]             c
//     [ BB2 ]       <                        \     /                          t
//        |           |                         \ /                            i
//        |           |                          v                             o
//        |           |                   [ lt-tail-2 ]                        n
//        |           |_________                 |                             |
//        v                                      v                             |
//      (...)                                  (...)                           V
//    --------------------------------------------------------------------------
namespace llvm {

#define DEBUG_TYPE "duplicate-bb"
  PreservedAnalyses DuplicateBBPass::run(Function &F, FunctionAnalysisManager &FAM) {
    if (!pRNG) {
      pRNG = F.getParent()->createRNG("duplicatre-bb");
    }
    BBToSingleRIVMap targets = findBBsToDuplicate(F, FAM.getResult<RivPass>(F));

    // This map is used to keep track of the new bindings. Otherwise, the
    // information from RIV will become obsolete.
    ValueToPhiMap ReMapper;

    // Duplicate
    // ctx == (bb, value)
    for (auto &BB_Ctx: targets) {
      cloneBB(*std::get<0>(BB_Ctx), std::get<1>(BB_Ctx), ReMapper);
    }

    return (targets.empty() ? llvm::PreservedAnalyses::all() : llvm::PreservedAnalyses::none());
  }

  DuplicateBBPass::BBToSingleRIVMap DuplicateBBPass::findBBsToDuplicate(Function &F, const RIVMap &RIVResult) {
    BBToSingleRIVMap blocksToDuplicate;

    for (BasicBlock &bb: F) {
      // handling exceptations is not considered
      if (bb.isLandingPad()) continue;

      auto const &reachableValues = RIVResult.lookup(&bb);
      size_t reachableValuesCount = reachableValues.size();

      if (reachableValuesCount == 0) {
        LLVM_DEBUG(errs() << "no context value in this basic block");
        continue;
      }

      // get randome context value from RIV set
      auto iter = reachableValues.begin();
      std::uniform_int_distribution<> dist(0, reachableValuesCount - 1);
      std::advance(iter, dist(*pRNG));// 将iter随机移动dist(*pRNG)个位置
      if (dyn_cast<GlobalValue>(*iter)) {
        LLVM_DEBUG(errs() << "Random context value is a global variable. "
                          << "Skipping this BB\n");
        continue;
      }

      LLVM_DEBUG(errs() << "Random context value: " << **iter << "\n");

      // Store the binding between the current BB and the context variable that
      // will be used for the `if-then-else` construct.
      blocksToDuplicate.emplace_back(&bb, *iter);
    }

    return blocksToDuplicate;
  }

  void DuplicateBBPass::cloneBB(BasicBlock &bb, Value *contextValue, ValueToPhiMap &reMapper) {
    BasicBlock::iterator BBHead = bb.getFirstNonPHIIt();// not duplicate phi node
    IRBuilder<> builder(&*BBHead);
    Value *cond = builder.CreateIsNull(
        reMapper.count(contextValue) ? reMapper[contextValue] : contextValue
    );

    // create and insert if then else
    // contain only one terminator
    Instruction *thenBlock = nullptr;
    Instruction *elseBlock = nullptr;
    SplitBlockAndInsertIfThenElse(cond, &*BBHead, &thenBlock, &elseBlock);
    BasicBlock *tail = thenBlock->getSuccessor(0);

    assert(tail == elseBlock->getSuccessor(0) && "Inconsist CFG");

    // give name to new BasicBlock
    std::string duplicatedBBId = std::to_string(duplicateBBCount);
    thenBlock->getParent()->setName("lt-clone-1-" + duplicatedBBId);
    elseBlock->getParent()->setName("lt-clone-2-" + duplicatedBBId);
    tail->setName("lt-tail-" + duplicatedBBId);
    thenBlock->getParent()->getSinglePredecessor()->setName("lt-if-then-else-" + duplicatedBBId);

    // Variables to keep track of the new bindings
    ValueToValueMapTy TailVMap, ThenVMap, ElseVMap;

    // The list of instructions in Tail that don't produce any values and thus
    // can be removed
    SmallVector<Instruction *, 8> ToRemove;

    // Iterate through the original basic block and clone every instruction into
    // the 'if-then' and 'else' branches. Update the bindings/uses on the fly
    // (through ThenVMap, ElseVMap, TailVMap). At this stage, all instructions
    // apart from PHI nodes, are stored in Tail.
    for (auto IIT = tail->begin(), IE = tail->end(); IIT != IE; ++IIT) {
      Instruction &Instr = *IIT;
      assert(!isa<PHINode>(&Instr) && "Phi nodes have already been filtered out");

      // Skip terminators - duplicating them wouldn't make sense unless we want
      // to delete Tail completely.
      if (Instr.isTerminator()) {
        RemapInstruction(&Instr, TailVMap, RF_IgnoreMissingLocals);
        continue;
      }

      // Clone the instructions.
      Instruction *ThenClone = Instr.clone(), *ElseClone = Instr.clone();

      // Operands of ThenClone still hold references to the original BB.
      // Update/remap them.
      // 克隆出来的指令操作数还指向原来的指令，需要更新成指向对应 clone 里的指令
      // 外部的值（函数参数、其他 BB 定义的值）不需要 remap
      // leaving VMap empty is fine
      RemapInstruction(ThenClone, ThenVMap, RF_IgnoreMissingLocals);
      ThenClone->insertBefore(thenBlock->getIterator());
      ThenVMap[&Instr] = ThenClone;

      // Operands of ElseClone still hold references to the original BB.
      // Update/remap them.
      RemapInstruction(ElseClone, ElseVMap, RF_IgnoreMissingLocals);
      ElseClone->insertBefore(elseBlock->getIterator());
      ElseVMap[&Instr] = ElseClone;

      // Instructions that don't produce values can be safely removed from Tail
      if (ThenClone->getType()->isVoidTy()) {
        ToRemove.push_back(&Instr);
        continue;
      }

      // Instruction that produce a value should not require a slot in the
      // TAIL *but* they can be used from the context, so just always
      // generate a PHI, and let further optimization do the cleaning
      PHINode *Phi = PHINode::Create(ThenClone->getType(), 2);
      Phi->addIncoming(ThenClone, thenBlock->getParent());
      Phi->addIncoming(ElseClone, elseBlock->getParent());
      TailVMap[&Instr] = Phi;

      reMapper[&Instr] = Phi;

      // Instructions are modified as we go, use the iterator version of
      // ReplaceInstWithInst.
      ReplaceInstWithInst(tail, IIT, Phi);
    }

    // Purge instructions that don't produce any value
    for (auto *I: ToRemove)
      I->eraseFromParent();

    ++duplicateBBCount;
  }


  extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
  llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "DuplicateBBPass", "v0.1",
        [](llvm::PassBuilder &PB) {
          // register for this counterpass
          PB.registerPipelineParsingCallback(
              [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                if (Name == "dupbbpass") {
                  FPM.addPass(DuplicateBBPass());
                  return true;
                }
                return false;
              }
          );
        }
    };
  }
}// namespace llvm

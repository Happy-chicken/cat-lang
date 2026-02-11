#include "mbapass.h"
#include "llvm-20/llvm/ADT/Statistic.h"
#include "llvm-20/llvm/Passes/PassBuilder.h"
#include "llvm-20/llvm/Passes/PassPlugin.h"
#include <llvm-20/llvm/IR/IRBuilder.h>
#include <llvm-20/llvm/IR/InstrTypes.h>
#include <llvm-20/llvm/IR/Instruction.h>
#include <llvm-20/llvm/Transforms/Utils/BasicBlockUtils.h>
namespace llvm {

#define DEBUG_TYPE "mba-sub"
  STATISTIC(SubstCount, "The # of substituted instructions");

  PreservedAnalyses
  MbaPass::run(Function &F, FunctionAnalysisManager &FAM) {
    bool changed = false;
    for (auto &BB: F) {
      changed |= runOnBasicBlock(BB);
    }
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool MbaPass::runOnBasicBlock(BasicBlock &bb) {
    bool changed = false;

    for (auto inst = bb.begin(); inst != bb.end(); ++inst) {
      auto binOp = dyn_cast<BinaryOperator>(inst);
      if (!binOp) continue;
      IRBuilder<> builder(binOp);

      // check inst is plus or minus
      unsigned opCode = binOp->getOpcode();
      if (binOp->getType()->isIntegerTy()) {
        switch (opCode) {
          // a - b == (a + ~b) + 1
          case Instruction::Sub: {
            // ::CreateAdd 创建游离的inst，等待后续使用（替换）
            // builder.CreateAdd 在builder位置处创建指令并插入到基本块中
            Instruction *newInst = BinaryOperator::CreateAdd(
                builder.CreateAdd(binOp->getOperand(0), builder.CreateNot(binOp->getOperand(1))),
                ConstantInt::get(binOp->getType(), 1)
            );
            // The following is visible only if you pass -debug on the command line
            // *and* you have an assert build.
            LLVM_DEBUG(dbgs() << *binOp << " -> " << *newInst << "\n");
            ReplaceInstWithInst(&bb, inst, newInst);
            changed = true;
            ++SubstCount;
            break;
          }
          // a + b == (((a ^ b) + 2 * (a & b)) * 39 + 23) * 151 + 111
          case Instruction::Add: {
            auto val39 = ConstantInt::get(binOp->getType(), 39);
            auto val151 = ConstantInt::get(binOp->getType(), 151);
            auto val23 = ConstantInt::get(binOp->getType(), 23);
            auto val2 = ConstantInt::get(binOp->getType(), 2);
            auto val111 = ConstantInt::get(binOp->getType(), 111);

            Instruction *newInst = BinaryOperator::CreateAdd(
                val111,
                builder.CreateMul(
                    val151,
                    builder.CreateAdd(
                        val23,
                        builder.CreateMul(
                            val39,
                            builder.CreateAdd(
                                builder.CreateXor(
                                    binOp->getOperand(0),
                                    binOp->getOperand(1)
                                ),
                                builder.CreateMul(
                                    val2,
                                    builder.CreateAnd(
                                        binOp->getOperand(0),
                                        binOp->getOperand(1)
                                    )
                                )
                            )
                        )
                    )
                )
            );
            LLVM_DEBUG(dbgs() << *binOp << " -> " << *newInst << "\n");
            ReplaceInstWithInst(&bb, inst, newInst);
            changed = true;
            ++SubstCount;
            break;
          }
          default:
            break;
        }
        continue;
      }
    }

    return changed;
  }

  extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
  llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MBA-SUB-ADD", "v0.1",
        [](llvm::PassBuilder &PB) {
          // register for this counterpass
          PB.registerPipelineParsingCallback(
              [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                if (Name == "mbapass") {
                  FPM.addPass(MbaPass());
                  return true;
                }
                return false;
              }
          );
        }
    };
  }
}// namespace llvm

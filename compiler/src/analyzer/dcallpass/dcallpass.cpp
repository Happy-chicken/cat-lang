#include "dcallpass.h"
#include "llvm-20/llvm/Passes/PassBuilder.h"
#include "llvm-20/llvm/Passes/PassPlugin.h"
#include <llvm-20/llvm/ADT/APInt.h>
#include <llvm-20/llvm/ADT/StringMap.h>
#include <llvm-20/llvm/ADT/StringRef.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/CallingConv.h>
#include <llvm-20/llvm/IR/Constant.h>
#include <llvm-20/llvm/IR/Constants.h>
#include <llvm-20/llvm/IR/DerivedTypes.h>
#include <llvm-20/llvm/IR/GlobalVariable.h>
#include <llvm-20/llvm/IR/IRBuilder.h>
#include <llvm-20/llvm/IR/Instructions.h>
#include <llvm-20/llvm/IR/Type.h>
#include <llvm-20/llvm/IR/Value.h>
#include <llvm-20/llvm/Support/Alignment.h>
#include <llvm-20/llvm/Support/Casting.h>
#include <llvm-20/llvm/Support/ModRef.h>
namespace llvm {

  static Constant *createGlobalCounter(Module &M, StringRef name) {
    auto &ctx = M.getContext();
    // insert a declaration into M
    // counter and newGV are actually the same thing
    Constant *counter = M.getOrInsertGlobal(name, IntegerType::getInt32Ty(ctx));
    GlobalVariable *newGV = M.getGlobalVariable(name);
    newGV->setLinkage(GlobalVariable::CommonLinkage);
    newGV->setAlignment(MaybeAlign(4));
    newGV->setInitializer(ConstantInt::get(ctx, APInt(32, 0)));
    return counter;
  }

  PreservedAnalyses DynamicCallPass::run(Module &M, ModuleAnalysisManager &MAM) {
    bool changed = runOnModule(M);
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool DynamicCallPass::runOnModule(Module &M) {
    bool instrumented = false;

    // function name -> dynamic counter
    StringMap<Constant *> callCounterMap;
    // function name -> function in llvm
    StringMap<Constant *> funcNameMap;

    auto &ctx = M.getContext();
    // for each funciton in Module, inject a variable for counting
    for (auto &F: M) {
      if (F.isDeclaration()) {
        continue;
      }
      // insert a IR Buidler at the top of function
      IRBuilder<> builder(&*F.getEntryBlock().getFirstInsertionPt());

      // create a global var to count function call
      std::string counterName = std::string("CounterFor_") + F.getName().data();
      Constant *var = createGlobalCounter(M, counterName);
      callCounterMap[F.getName()] = var;

      auto funcName = builder.CreateGlobalString(F.getName());
      funcNameMap[F.getName()] = funcName;

      // inject
      LoadInst *load2 = builder.CreateLoad(IntegerType::getInt32Ty(ctx), var);
      // add it with one: var++
      Value *inc2 = builder.CreateAdd(builder.getInt32(1), load2);
      builder.CreateStore(inc2, var);

      instrumented = true;
    }

    if (!instrumented) return instrumented;

    // inject declaration of printf("\n");
    PointerType *printfArgTy = PointerType::getUnqual(ctx);
    FunctionType *printfTy = FunctionType::get(IntegerType::getInt32Ty(ctx), printfArgTy, true);
    FunctionCallee Printf = M.getOrInsertFunction("printf", printfTy);

    // set attribute
    Function *readOnlyPrintf = dyn_cast<Function>(Printf.getCallee());
    readOnlyPrintf->setDoesNotThrow();
    readOnlyPrintf->addParamAttr(0, Attribute::NoCapture);
    readOnlyPrintf->addParamAttr(0, Attribute::ReadOnly);

    // inject global string that holds print format
    Constant *formatStr = ConstantDataArray::getString(ctx, "%-20s %-10lu\n");
    // 通过这个formatstrvar来访问string字面量
    Constant *formatStrVar = M.getOrInsertGlobal("PrintFormatIR", formatStr->getType());
    dyn_cast<GlobalVariable>(formatStrVar)->setInitializer(formatStr);

    std::string out = "";
    out += "=================================================\n";
    out += "LLVM: dynamic analysis results\n";
    out += "=================================================\n";
    out += "NAME                 #N DIRECT CALLS\n";
    out += "-------------------------------------------------\n";

    Constant *headerStr = ConstantDataArray::getString(ctx, out.c_str());
    Constant *headerStrVar = M.getOrInsertGlobal("PrintHeaderIR", headerStr->getType());
    dyn_cast<GlobalVariable>(headerStrVar)->setInitializer(headerStr);

    //define print wrapper
    FunctionType *printWrapperTy = FunctionType::get(Type::getVoidTy(ctx), {}, false);
    Function *printWrapper = dyn_cast<Function>(M.getOrInsertFunction(
                                                     "printf_wrapper",
                                                     printWrapperTy
    )
                                                    .getCallee());
    // create entry
    BasicBlock *retBlock = BasicBlock::Create(ctx, "enter", printWrapper);
    IRBuilder<> builder(retBlock);
    // start insrting call to printf
    Value *resultHeaderStrPtr = builder.CreatePointerCast(headerStrVar, printfArgTy);
    Value *resultFormatStrPtr = builder.CreatePointerCast(formatStrVar, printfArgTy);

    builder.CreateCall(Printf, {resultHeaderStrPtr});
    LoadInst *loadCounter;
    for (auto &item: callCounterMap) {
      loadCounter = builder.CreateLoad(IntegerType::getInt32Ty(ctx), item.second);
      builder.CreateCall(Printf, {resultFormatStrPtr, funcNameMap[item.first()], loadCounter});
    }
    builder.CreateRetVoid();

    // call print wrapper

    return instrumented;
  }

  extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "DCallCounter", "v0.1",
        [](llvm::PassBuilder &PB) {
          // register for this counterpass
          PB.registerPipelineParsingCallback(
              [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                if (Name == "dcpass>") {
                  MPM.addPass(DynamicCallPass());
                  return true;
                }
                return false;
              }
          );
          // PB.registerVectorizerStartEPCallback(
          //     [](llvm::FunctionPassManager &PM,
          //        llvm::OptimizationLevel Level) {
          //         PM.addPass(PrinterPass(llvm::errs()));
          //     }
          // );
          // register for OpCodePrinter can request for this counterpass
        }
    };
  }
}// namespace llvm

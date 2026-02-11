#include "injectfuncpass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/IR/Analysis.h>
#include <llvm/IR/PassManager.h>

namespace llvm {
    PreservedAnalyses InjectFuncPass::run(Module &M, ModuleAnalysisManager &MAM) {
        bool changed = runOnModule(M);
        return (changed) ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    bool InjectFuncPass::runOnModule(Module &M) {
        bool insertAtLeastOnePrintf = false;
        auto &ctx = M.getContext();
        PointerType *printfArgTy = PointerType::getUnqual(ctx);

        // inject declaration of printf("\n");
        FunctionType *printfTy = FunctionType::get(
            IntegerType::getInt32Ty(ctx),
            printfArgTy,
            true
        );
        FunctionCallee printF = M.getOrInsertFunction("printf", printfTy);

        Function *PrintF = dyn_cast<Function>(printF.getCallee());
        PrintF->setDoesNotThrow();
        PrintF->addParamAttr(0, llvm::Attribute::ReadOnly);

        // Inject a global var to hold printf format string
        Constant *printfFormatStr = ConstantDataArray::getString(
            ctx,
            "Hello from %s\nnumber of arguments%d\n"
        );
        Constant *printfFormatStrVar = M.getOrInsertGlobal("PrintfFormatStr", printfFormatStr->getType());
        dyn_cast<GlobalVariable>(printfFormatStrVar)->setInitializer(printfFormatStr);

        // for each function in module, inject a call to pritnf
        for (auto &F: M) {
            if (F.isDeclaration()) continue;
            // get a ir builder and set it to the top of function
            IRBuilder<> builder(&*F.getEntryBlock().getFirstInsertionPt());
            // inject(creeate) a function name in global
            auto funcName = builder.CreateGlobalString(F.getName());
            // printf holds i8*, but we have an array [n x i8], we need cast it
            Value *formatStrPtr = builder.CreatePointerCast(printfFormatStrVar, printfArgTy, "format_str");

            // debug

            // inject call
            builder.CreateCall(printF, {formatStrPtr, funcName, builder.getInt32(F.arg_size())});
            insertAtLeastOnePrintf = true;
        }
        return insertAtLeastOnePrintf;
    }

    // register
    PassPluginLibraryInfo getInjectFuncPassPluginInfo() {
        return {
            LLVM_PLUGIN_API_VERSION, "injectfuncpass", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
                        if (name == "injectfuncpass") {
                            MPM.addPass(InjectFuncPass());
                            return true;
                        }
                        return false;
                    }
                );
            }
        };
    }
    extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
        return getInjectFuncPassPluginInfo();
    }


}// namespace llvm

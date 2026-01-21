#include "Optimizer.hpp"
#include <llvm/IR/PassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SCCP.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

Optimizier::Optimizier() : passBuilder() {
    // register all the basic analyses with the managers
    passBuilder.registerModuleAnalyses(moduleAM);
    passBuilder.registerCGSCCAnalyses(cgsccAM);
    passBuilder.registerFunctionAnalyses(functionAM);
    passBuilder.registerLoopAnalyses(loopAM);
    passBuilder.crossRegisterProxies(loopAM, functionAM, cgsccAM, moduleAM);
    // functionAM.registerPass([&]() {
    //     return passBuilder.buildFunctionSimplificationPipeline(llvm::OptimizationLevel::O3);
    // });
}

void Optimizier::optimize(llvm::Module &module, llvm::OptimizationLevel optLevel) {
    llvm::ModulePassManager modulePM;
    llvm::FunctionPassManager functionPM;

    // O0
    if (optLevel != llvm::OptimizationLevel::O0) {
        functionPM.addPass(llvm::SimplifyCFGPass());// simplify the control flow graph
        functionPM.addPass(llvm::InstCombinePass());// combine instructions to form fewer, simpler instructions
        modulePM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(functionPM)));
        modulePM.addPass(passBuilder.buildPerModuleDefaultPipeline(optLevel));// default optimization pipeline
    }
    modulePM.run(module, moduleAM);
}
#include "./counterpass.h"

#include "llvm/Passes/PassPlugin.h"
#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Analysis.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
using namespace llvm;
#define DEBUG_TYPE "counterpass"

STATISTIC(NumOfInst, "Number of instructions");
STATISTIC(NumofBB, "Number of Basic Block");
llvm::AnalysisKey CounterPass::Key;
static void printOpcodeCounterResult(llvm::raw_ostream &out, const CounterPass::Result &opcodeMap) {
    out << "================================================="
        << "\n";
    out << "LLVM: OpcodeCounter results\n";
    out << "=================================================\n";
    const char *str1 = "OPCODE";
    const char *str2 = "#TIMES USED";
    out << format("%-20s %-10s\n", str1, str2);
    out << "-------------------------------------------------"
        << "\n";
    for (auto &Inst: opcodeMap) {
        out << format("%-20s %-10lu\n", Inst.first().str().c_str(), Inst.second);
    }
    out << "-------------------------------------------------"

        << "\n\n";
}
CounterPass::Result CounterPass::generateOpcodeMap(llvm::Function &F) {
    CounterPass::Result opcodeMap;
    llvm::errs() << "Function '" << F.getName() << "' has " << F.arg_size() << " arg(s)\n";
    for (auto &BB: F) {
        NumofBB++;
        for (auto &I: BB) {
            StringRef instName = I.getOpcodeName();
            if (opcodeMap.find(instName) != opcodeMap.end()) {
                opcodeMap[instName]++;
            } else {
                opcodeMap[instName] = 0;
            }
            NumOfInst++;
        }
    }
    return opcodeMap;
}
// Register the pass
CounterPass::Result CounterPass::run(Function &F, FunctionAnalysisManager &FAM) {
    llvm::errs() << "Basic Block " << NumofBB << "\n";
    llvm::errs() << "Nuber of instructions: " << NumOfInst << "\n";
    return generateOpcodeMap(F);
}

PreservedAnalyses PrinterPass::run(Function &F, FunctionAnalysisManager &FAM) {
    auto &opcodeMap = FAM.getResult<CounterPass>(F);
    os << "Print Counter Pass for Function '"
       << F.getName() << "': \n";
    printOpcodeCounterResult(os, opcodeMap);
    return PreservedAnalyses::all();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "Counter", "v0.1",
        [](llvm::PassBuilder &PB) {
            // register for this counterpass
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if (Name == "print<counter>") {
                        FPM.addPass(PrinterPass(llvm::errs()));
                        return true;
                    }
                    return false;
                }
            );
            PB.registerVectorizerStartEPCallback(
                [](llvm::FunctionPassManager &PM,
                   llvm::OptimizationLevel Level) {
                    PM.addPass(PrinterPass(llvm::errs()));
                }
            );
            // register for OpCodePrinter can request for this counterpass
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                    FAM.registerPass([&] { return CounterPass(); });
                }
            );
        }
    };
}

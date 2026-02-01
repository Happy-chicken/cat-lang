#pragma once
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include <limits.h>
#include <llvm-20/llvm/ADT/StringMap.h>
#include <llvm-20/llvm/ADT/StringRef.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/Intrinsics.h>
#include <llvm-20/llvm/Support/raw_ostream.h>
namespace llvm {

    class CounterPass : public AnalysisInfoMixin<CounterPass> {
    public:
        using Result = llvm::StringMap<unsigned>;
        Result run(Function &F, FunctionAnalysisManager &FAM);
        CounterPass::Result generateOpcodeMap(llvm::Function &F);
        static bool isRequired() { return true; }
        static StringRef name() { return "CounterPass"; }

    private:
        static llvm::AnalysisKey Key;
        friend struct AnalysisInfoMixin<CounterPass>;
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

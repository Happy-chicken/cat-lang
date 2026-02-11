#pragma once

// this pass integrates static call counter which only counts function calls in compilation time
#include <llvm-20/llvm/ADT/MapVector.h>
#include <llvm-20/llvm/IR/Analysis.h>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm-20/llvm/IR/Module.h>
#include <llvm-20/llvm/IR/PassManager.h>
#include <llvm-20/llvm/Support/raw_ostream.h>
using namespace llvm;
class StaticCallPass : public AnalysisInfoMixin<StaticCallPass> {
  public:
  using Result = MapVector<const Function *, unsigned>;
  Result run(Module &M, ModuleAnalysisManager &MAM);
  Result runOnModule(Module &M);

  static bool isRequired() { return true; }

  private:
  static AnalysisKey Key;
  friend class AnalysisInfoMixin<StaticCallPass>;
};

class PrinterPass : public PassInfoMixin<PrinterPass> {
  public:
  explicit PrinterPass(raw_ostream &out) : os(out) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
  static StringRef name() { return "PrinterPass"; }

  private:
  raw_ostream &os;
};

#pragma once
#include "CodeGen.hpp"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/Support/InitLLVM.h>
#include <memory>
template<typename T>
using uptr = std::unique_ptr<T>;
template<typename T>
using sptr = std::shared_ptr<T>;

using namespace llvm::orc;
class JIT {
  private:
  CodeGen &ir_generator;

  public:
  // static llvm::cl::opt<std::string> InputFile(llvm::cl::Positional, llvm::cl::Required, llvm::cl::desc("<input-file >"));
  JIT(CodeGen &ir_generator_) : ir_generator(ir_generator_) {}
  uptr<llvm::Module> loadModule();
  llvm::Error run(uptr<llvm::Module> module, uptr<llvm::LLVMContext> ctx, int argc, char *argv[]);
};

//using orc api to build a jit from scratch
/*
Basic procedure:
1. initialize the execution engine instance
2. initialize the layers, including RTDyldObjectLinkingLayer and IRCompileLayer
3. create JITDylib symbol table
4. add the module to the symbol table
5. search the first function symbol -> main
5. execute the main function
*/
using namespace llvm::orc;
class CatJIT {
  private:
  uptr<ExecutorProcessControl> EPC;
  uptr<ExecutionSession> ES;
  llvm::DataLayout DL;
  MangleAndInterner Mangle;
  uptr<RTDyldObjectLinkingLayer> ObjectLinkingLayer;
  uptr<IRCompileLayer> CompileLayer;
  uptr<IRTransformLayer> OptimizeLayer;
  JITDylib &MainJitDylib;

  public:
  ~CatJIT() {
    if (auto Err = MainJitDylib.clear()) {
      llvm::errs() << "Error clearing MainJitDylib: " << Err << "\n";
    }
  }

  llvm::orc::JITDylib &getMainJITDylib() {
    return MainJitDylib;
  }
  const llvm::DataLayout &getDataLayout() const {
    return DL;
  }
  // create a static factory method to hanle error before creating the instance
  // TODO: customize JIT
  static llvm::Expected<uptr<CatJIT>> Create() {
    // implement string internalization and shared with several class
    auto SSP = std::make_shared<SymbolStringPool>();
    // To take control of the current process, we create a SelfTargetProcessControl instance.
    // If we want to target a different process, then we need to change this instance
    auto SEPC = SelfExecutorProcessControl::Create(SSP);
    if (!SEPC) {
      return SEPC.takeError();
    }
    JITTargetMachineBuilder JITTMBuilder((*SEPC)->getTargetTriple());
    auto DL = JITTMBuilder.getDefaultDataLayoutForTarget();
    if (!DL) {
      return DL.takeError();
    }

    auto ES = std::make_unique<ExecutionSession>(std::move(*SEPC));
    return std::make_unique<CatJIT>(std::move(*SEPC), std::move(ES), std::move(*DL), std::move(JITTMBuilder));
  }

  CatJIT(uptr<ExecutorProcessControl> EPCtrl, uptr<ExecutionSession> ExeS, llvm::DataLayout DataL, JITTargetMachineBuilder JTMB)
      : EPC(std::move(EPCtrl)), ES(std::move(ExeS)), DL(std::move(DataL)),
        Mangle(*ES, DL),
        ObjectLinkingLayer(std::move(createObjectLinkingLayer(*ES, JTMB))),
        CompileLayer(std::move(createCompileLayer(*ES, *ObjectLinkingLayer, std::move(JTMB)))),
        OptimizeLayer(std::move(createOptIRLayer(*ES, *CompileLayer))),
        MainJitDylib(ES->createBareJITDylib("<main>")) {
    MainJitDylib.addGenerator(llvm::cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));
  }

  // To create an object-linking layer, we need to provide a memory manager.
  static uptr<RTDyldObjectLinkingLayer> createObjectLinkingLayer(ExecutionSession &ES, JITTargetMachineBuilder &JTMB) {
    auto GetMemoryManager = []() { return std::make_unique<llvm::SectionMemoryManager>(); };
    auto OLLayer = std::make_unique<RTDyldObjectLinkingLayer>(ES, GetMemoryManager);
    if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
      OLLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
      OLLayer->setAutoClaimResponsibilityForObjectSymbols(true);
    }
    return OLLayer;
  }

  // The IRCompiler instance is responsible for compiling an IR module into an object file.
  // If our JIT compiler does not use threads, then we can use the SimpleCompiler class,
  // which compiles the IR module using a given target machine.
  // The TargetMachine class is not threadsafe, and therefore the SimpleCompiler class is not, either.
  // To support compilation with multiple threads, we use the ConcurrentIRCompiler class,
  // which creates a new TargetMachine instance for each module to compile.
  static uptr<IRCompileLayer> createCompileLayer(ExecutionSession &ES, RTDyldObjectLinkingLayer &OLLayer, JITTargetMachineBuilder JTMB) {
    auto IRCompiler = std::make_unique<ConcurrentIRCompiler>(std::move(JTMB));
    auto IRCLayer = std::make_unique<IRCompileLayer>(ES, OLLayer, std::move(IRCompiler));
    return IRCLayer;
  }

  // using opt layer instead of translate IR to machine code
  static uptr<llvm::orc::IRTransformLayer> createOptIRLayer(llvm::orc::ExecutionSession &ES, llvm::orc::IRCompileLayer &CompileLayer) {
    // The optimizeModule() function is an example of a transformation on an IR module.
    auto OptIRLayer = std::make_unique<llvm::orc::IRTransformLayer>(ES, CompileLayer, optimizeModule);
    return OptIRLayer;
  }
  static llvm::Expected<llvm::orc::ThreadSafeModule> optimizeModule(llvm::orc::ThreadSafeModule TSM, const llvm::orc::MaterializationResponsibility &R) {
    TSM.withModuleDo([](llvm::Module &M) { 
            // bool DebugPM = false; 
            llvm::PassBuilder PB; 
            llvm::LoopAnalysisManager LAM; 
            llvm::FunctionAnalysisManager FAM; 
            llvm::CGSCCAnalysisManager CGAM; 
            llvm::ModuleAnalysisManager MAM; 
            FAM.registerPass( [&] { 
                return PB.buildDefaultAAPipeline(); 
            }); 
                PB.registerModuleAnalyses(MAM); 
                PB.registerCGSCCAnalyses(CGAM); 
                PB.registerFunctionAnalyses(FAM); 
                PB.registerLoopAnalyses(LAM); 
                PB.crossRegisterProxies(LAM, FAM, CGAM, MAM); 
                llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2); 
                MPM.run(M, MAM); });
    return TSM;
  }

  llvm::Error addIRModule(llvm::orc::ThreadSafeModule TSM, llvm::orc::ResourceTrackerSP RT = nullptr) {
    if (!RT) {
      RT = MainJitDylib.getDefaultResourceTracker();
    }
    return OptimizeLayer->add(RT, std::move(TSM));
  }

  llvm::Expected<ExecutorSymbolDef> lookup(llvm::StringRef Name) {
    return ES->lookup({&MainJitDylib}, Mangle(Name.data()));
  }
};

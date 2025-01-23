// HelloJIT.cpp

#include "llvm-14/llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm-14/llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm-14/llvm/IR/BasicBlock.h"
#include "llvm-14/llvm/IR/Function.h"
#include "llvm-14/llvm/IR/IRBuilder.h"
#include "llvm-14/llvm/IR/LLVMContext.h"
#include "llvm-14/llvm/IR/Module.h"
#include "llvm-14/llvm/IR/Verifier.h"
#include "llvm-14/llvm/Support/InitLLVM.h"
#include "llvm-14/llvm/Support/TargetSelect.h"
#include "llvm-14/llvm/Support/raw_ostream.h"

#include <iostream>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

typedef int (*AddFunctionType)(int, int);

// Create a module to represent the following C function
//   int Add(int a, int b)
//   {
//       return a + b;
//   }
ThreadSafeModule CreateModule() {
    auto context = std::make_unique<LLVMContext>();
    IRBuilder<> builder(*context);

    // Create a module
    auto module = std::make_unique<Module>("Add.c", *context);

    // Create a function that looks like:
    //   int Add(int a, int b)
    std::vector<Type *> parameters(2, builder.getInt32Ty());
    FunctionType *functionType = FunctionType::get(builder.getInt32Ty(), parameters, false);
    Function *function = Function::Create(functionType, GlobalValue::ExternalLinkage, "Add", module.get());

    // Set arguments for the function
    function->getArg(0)->setName("a");
    function->getArg(1)->setName("b");

    // Create a block in the function
    BasicBlock *block = BasicBlock::Create(*context, "entry", function);
    builder.SetInsertPoint(block);

    // Create an instruction to add a and b:
    //   return a + b;
    Value *a = function->getArg(0);
    Value *b = function->getArg(1);
    Value *result = builder.CreateAdd(a, b, "result");
    builder.CreateRet(result);

    // Print the IR
    verifyFunction(*function);
    module->print(outs(), nullptr);

    return ThreadSafeModule(std::move(module), std::move(context));
}

// Compile and run the "Add()" function
int main(int argc, char *argv[]) {
    // Do initialization for JIT
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // Create the JIT
    ExitOnError ExitOnErr;
    auto jit = ExitOnErr(LLJITBuilder().create());
    auto module = CreateModule();
    ExitOnErr(jit->addIRModule(std::move(module)));

    // Find the "Add()" function
    auto functionSymbol = ExitOnErr(jit->lookup("Add"));
    AddFunctionType add = (AddFunctionType) functionSymbol.getAddress();

    // Use the "Add()" function
    int result = add(12, 34);
    std::cout << "\n-----------------\n";
    std::cout << "Add(12, 34) = " << result << std::endl;

    return 0;
}

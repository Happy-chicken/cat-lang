#pragma once
#include "AST.hpp"
#include "Environment.hpp"
#include "Symbol.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
template<typename T>
using uptr = std::unique_ptr<T>;

struct ActiveFuncState {
    const FuncSymbol *funcSym;
    llvm::Value *framePtr = nullptr;                                // Pointer to the current function's frame
    llvm::Value *staticLinkPtr = nullptr;                           // Pointer to the parent frame
    std::unordered_map<const Symbol *, llvm::Value *> localVarAddrs;// Local variables address(i.e that var) map

    struct LoopInfo {
        llvm::BasicBlock *breakBB = nullptr;
        llvm::BasicBlock *continueBB = nullptr;
    };
    std::vector<LoopInfo> loopStack;// Stack to manage nested loops
};

struct FuncSignature {
    vec<llvm::Type *> paramTys;
    llvm::Type *retTy;
};

class CodeGenCtx {
public:
    using FieldMap = std::unordered_map<std::string, llvm::Type *>;
    using MethodMap = std::unordered_map<std::string, llvm::Function *>;
    static llvm::Function *curFunction;

    CodeGenCtx(const std::string &moduleName) : ctx(std::make_unique<llvm::LLVMContext>()),
                                                module(std::make_unique<llvm::Module>(moduleName, *ctx)),
                                                builder(std::make_unique<llvm::IRBuilder<>>(*ctx)) {
        module->setTargetTriple("x86_64-pc-linux-gnu");
    }
    ~CodeGenCtx() = default;

    llvm::LLVMContext &getLLVMContext() { return *ctx; }
    const llvm::LLVMContext &getLLVMContext() const { return *ctx; }
    uptr<llvm::LLVMContext> releaseLLVMContext() { return std::move(ctx); }
    llvm::Module &getModule() { return *module; }
    const llvm::Module &getModule() const { return *module; }
    uptr<llvm::Module> releaseModule() { return std::move(module); }
    llvm::IRBuilder<> &getBuilder() { return *builder; }
    const llvm::IRBuilder<> &getBuilder() const { return *builder; }
    llvm::IRBuilder<> &getVarsBuilder() { return *varsBuilder; }


private:
    uptr<llvm::LLVMContext> ctx;
    uptr<llvm::Module> module;
    uptr<llvm::IRBuilder<>> builder;
    uptr<llvm::IRBuilder<>> varsBuilder;// this builder always prepends to the beginning of the function entry block


    // class information
    struct ClassInfo {
        llvm::StructType *cls;
        llvm::StructType *parent;
        FieldMap fieldsMap;
        MethodMap methodsMap;
    };


    using ClassMap = std::unordered_map<const Symbol *, uptr<ClassInfo>>;
    vec<ActiveFuncState> funcStack; // Stack of active function states
    ClassMap classMap;              // Map of class symbols to their LLVM struct types
    llvm::StructType *cls = nullptr;// current class


public:
    // Type Translation
    llvm::Type *getLLVMType(const SemaType &ty, bool forParam = false);
    llvm::GlobalVariable *createGlobalVariable(const std::string &name, llvm::Constant *init);

    llvm::Value *createLocalVariable(Symbol *sym, llvm::Type *type, Environment::Env env);
    llvm::Function *createFunction(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env);
    llvm::Function *createFunctionProto(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env);

    void createFunctionBlock(llvm::Function *fn);                                             // create a function block
    llvm::BasicBlock *createBasicBlock(const std::string &bbName, llvm::Function *parentFunc);// create a basic block


    // methods related to class
    // TODO
    // void inheritClass(llvm::StructType *cls, llvm::StructType *parent);         // inherit parent class field
    void buildClassInfo(llvm::StructType *cls, const ClassDef &clsStmt);        // build class info
    void buildClassBody(llvm::StructType *cls);                                 // build class body
    void buildVTable(llvm::StructType *cls, ClassInfo *classInfo);              // build vtable
    size_t getFieldIndex(llvm::StructType *cls, const std::string &fieldName);  // get field index
    size_t getMethodIndex(llvm::StructType *cls, const std::string &methodName);// get method index
    bool isMethod(llvm::StructType *cls, const std::string &name);              // check if a function is a method
};
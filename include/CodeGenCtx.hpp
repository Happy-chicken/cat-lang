#pragma once
#include "Environment.hpp"
#include "SemanticCtx.hpp"
#include "Symbol.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm-20/llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <utility>
template<typename T>
using uptr = std::unique_ptr<T>;
static const size_t RESERVED_FIELD_COUNT = 1;
static const size_t VTABLE_INDEX = 0;

struct ActiveFuncState {
    const FuncSymbol *funcSym;
    llvm::Value *framePtr = nullptr;     // Pointer to the current function's frame
    llvm::Value *staticLinkPtr = nullptr;// Pointer to the parent frame
    std::unordered_map<const Symbol *, llvm::Value *>
        localVarAddrs;// Local variables address(i.e that var) map

    struct LoopInfo {
        llvm::BasicBlock *breakBB = nullptr;
        llvm::BasicBlock *continueBB = nullptr;
    };
    std::vector<LoopInfo>
        loopStack;// Stack to manage nested truct FuncSignature {
    vec<llvm::Type *> paramTys;
    llvm::Type *retTy;
};

class CodeGenCtx {
public:
    using FieldMap = std::unordered_map<std::string, llvm::Type *>;
    using MethodMap = std::unordered_map<std::string, llvm::Function *>;
    static llvm::Function *curFunction;

    CodeGenCtx(const std::string &moduleName)
        : ctx(std::make_unique<llvm::LLVMContext>()),
          module(std::make_unique<llvm::Module>(moduleName, *ctx)),
          builder(std::make_unique<llvm::IRBuilder<>>(*ctx)) {
        module->setTargetTriple("x86_64-pc-linux-gnu");
        module->getOrInsertFunction("malloc", llvm::FunctionType::get(llvm::PointerType::get(*ctx, 0), builder->getInt64Ty(), false));
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

    // class information
    struct ClassInfo {
        ClassInfo(llvm::StructType *clsTy, llvm::StructType *parTy, FieldMap &fmap, MethodMap &mmap)
            : cls(clsTy), parent(parTy), fieldsMap(fmap), methodsMap(mmap) {}
        ClassInfo(llvm::StructType *clsTy, llvm::StructType *party)
            : cls(clsTy), parent(party) {
            fieldsMap = {};
            methodsMap = {};
        }
        llvm::StructType *cls;
        llvm::StructType *parent;
        FieldMap fieldsMap;
        MethodMap methodsMap;
    };

    struct FuncSignature {
        vec<llvm::Type *> paramTys;
        llvm::Type *retTy;
    };
    llvm::StructType *curCls = nullptr;// current class
    llvm::Value *curThisCls = nullptr; // current this Pointer of current class
private:
    uptr<llvm::LLVMContext> ctx;
    uptr<llvm::Module> module;
    uptr<llvm::IRBuilder<>> builder;
    uptr<llvm::IRBuilder<>>
        varsBuilder;// this builder always prepends to the beginning of the
                    // function entry block

    using ClassMap = std::unordered_map<string, uptr<ClassInfo>>;
    vec<ActiveFuncState> funcStack;// Stack of active function states
    ClassMap classMap;             // Map of class symbols to their LLVM struct types

public:
    void addClsMap(string clsName, uptr<ClassInfo> clsInfo) {
        classMap[clsName] = std::move(clsInfo);
    }
    ClassInfo *lookupClsMap(string clsName) {
        if (classMap.find(clsName) != classMap.end()) {
            return classMap[clsName].get();
        }
        return nullptr;
    }

public:
    // Type Translation
    llvm::Type *getLLVMType(const SemaType &ty, bool forParam = false);
    llvm::GlobalVariable *createGlobalVariable(const std::string &name, llvm::Constant *init);

    llvm::Value *createLocalVariable(Symbol *sym, llvm::Type *type, Environment::Env env);
    llvm::Function *createFunction(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env);
    llvm::Function *createFunctionProto(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env);

    void createFunctionBlock(llvm::Function *fn);// create a function block
    llvm::BasicBlock *
    createBasicBlock(const std::string &bbName,
                     llvm::Function *parentFunc);// create a basic block

    // methods related to class
    // TODO
    // void inheritClass(llvm::StructType *cls, llvm::StructType *parent); //
    // inherit parent class field
    size_t getTypeSize(llvm::Type *type);
    llvm::Value *createInstance(const string &clsName, const string &varName, Environment::Env env);
    llvm::Value *
    mallocInstance(llvm::StructType *cls,
                   const std::string &name);// allocate an object of a given class on the heap
    void buildClassInfo(llvm::StructType *cls, const ClassDef &clsStmt,
                        Environment::Env env); // build class info
    void buildClassBody(llvm::StructType *cls);// build class body
    void buildVTable(llvm::StructType *cls,
                     ClassInfo *classInfo);// build vtable
    size_t getFieldIndex(llvm::StructType *cls,
                         const std::string &fieldName);// get field index
    size_t getMethodIndex(llvm::StructType *cls,
                          const std::string &methodName);// get method index
    bool isMethod(llvm::StructType *cls,
                  const std::string &name);// check if a function is a method

    // helper
    FuncSignature buildSignature(const FuncSymbol *funcSym, bool isMain = false, bool isMethod = false);
};

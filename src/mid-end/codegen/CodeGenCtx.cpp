#include "CodeGenCtx.hpp"
#include "AST.hpp"
#include "SemaType.hpp"
#include <cstddef>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
llvm::Function *CodeGenCtx::curFunction = nullptr;

llvm::Type *CodeGenCtx::getLLVMType(const SemaType &ty, bool forParam) {
    auto typeKind = ty.getKind();
    switch (typeKind) {
        case SemaType::TypeKind::INT:
            return llvm::Type::getInt32Ty(getLLVMContext());
        case SemaType::TypeKind::CHAR:
            return llvm::Type::getInt8Ty(getLLVMContext());
        case SemaType::TypeKind::BYTE:
            return llvm::Type::getInt8Ty(getLLVMContext());
        case SemaType::TypeKind::BOOL:
            return llvm::Type::getInt1Ty(getLLVMContext());
        case SemaType::TypeKind::STR:
            return llvm::PointerType::get(llvm::Type::getInt8Ty(getLLVMContext()), 0);
        // return nullptr;
        case SemaType::TypeKind::VOID:
            return llvm::Type::getVoidTy(getLLVMContext());
        case SemaType::TypeKind::ARRAY: {
            const ArrayType &arrayTy = static_cast<const ArrayType &>(ty);
            auto elemType = arrayTy.elementType();
            if (!elemType) {
                return nullptr;
            }
            llvm::Type *elemTy = getLLVMType(*elemType, false);
            if (!arrayTy.size().has_value()) {
                return llvm::PointerType::get(elemTy, 0);
            }
            auto llvmArrayType = llvm::ArrayType::get(elemTy, *arrayTy.size());
            return llvmArrayType;
        }
        case SemaType::TypeKind::FUNC: {
            const auto &funcTy = static_cast<const FuncType &>(ty);
            std::vector<llvm::Type *> paramTypes;
            for (const auto &paramType: funcTy.params()) {
                if (paramType) {
                    llvm::Type *llvmParamType = getLLVMType(*paramType, true);
                    paramTypes.push_back(llvmParamType);
                }
            }
            auto *returnType = funcTy.returnType() ? getLLVMType(*funcTy.returnType(), false)
                                                   : llvm::Type::getVoidTy(getLLVMContext());
            auto llvmFuncType = llvm::FunctionType::get(returnType, paramTypes, false);
            return llvmFuncType;
        }
        case SemaType::TypeKind::CLS:
            break;
        case SemaType::TypeKind::INST:
            break;
    }
    return nullptr;
}
llvm::GlobalVariable *CodeGenCtx::createGlobalVariable(const std::string &name, llvm::Constant *init) {
    module->getOrInsertGlobal(name, init->getType());
    auto variable = module->getNamedGlobal(name);
    variable->setConstant(false);
    variable->setInitializer(init);
    return variable;
}

llvm::Value *CodeGenCtx::createLocalVariable(Symbol *sym, llvm::Type *type, Environment::Env env) {
    // Save current insertion point
    auto savedInsertBlock = builder->GetInsertBlock();
    auto savedInsertPoint = builder->GetInsertPoint();

    // Get entry block and insert alloca at the beginning
    llvm::BasicBlock &entryBlock = curFunction->getEntryBlock();
    builder->SetInsertPoint(&entryBlock, entryBlock.begin());

    // Create alloca instruction
    auto varAlloc = builder->CreateAlloca(type, nullptr, sym->getName());

    // Restore insertion point to continue generating code where we left off
    builder->SetInsertPoint(savedInsertBlock, savedInsertPoint);

    env->bind(sym, varAlloc);
    return varAlloc;
}

llvm::Function *CodeGenCtx::createFunction(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env) {
    auto func = module->getFunction(funcSym->getName());
    if (!func) {
        func = createFunctionProto(funcSym, fnType, env);
    }
    createFunctionBlock(func);
    return func;
}

llvm::Function *CodeGenCtx ::createFunctionProto(const FuncSymbol *funcSym, llvm::FunctionType *fnType, Environment::Env env) {
    auto func = llvm::Function::Create(
        fnType,
        llvm::GlobalValue::ExternalLinkage,
        funcSym->getName(),
        module.get()
    );
    verifyFunction(*func);
    env->bindFunc(funcSym, func);
    return func;
}

void CodeGenCtx::createFunctionBlock(llvm::Function *fn) {
    auto entry = createBasicBlock("entry", fn);
    builder->SetInsertPoint(entry);
}

llvm::BasicBlock *CodeGenCtx::createBasicBlock(const std::string &bbName, llvm::Function *parentFunc) {
    return llvm::BasicBlock::Create(getLLVMContext(), bbName, parentFunc);
}


// inherit parent class field
void CodeGenCtx::buildClassInfo(llvm::StructType *cls, const ClassDef &clsStmt) {
}
void CodeGenCtx::buildClassBody(llvm::StructType *cls) {
}
void CodeGenCtx::buildVTable(llvm::StructType *cls, ClassInfo *classInfo) {
}
size_t CodeGenCtx::getFieldIndex(llvm::StructType *cls, const std::string &fieldName) {
    return 0;
}
size_t CodeGenCtx::getMethodIndex(llvm::StructType *cls, const std::string &methodName) {
    return 0;
}
bool CodeGenCtx::isMethod(llvm::StructType *cls, const std::string &name) {
    return false;
}
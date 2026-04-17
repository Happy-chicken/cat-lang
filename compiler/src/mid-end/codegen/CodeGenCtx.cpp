#include "CodeGenCtx.hpp"
#include "AST.hpp"
#include "Environment.hpp"
#include "SemaType.hpp"
#include "Symbol.hpp"
#include <cstddef>
#include <iterator>
#include <llvm-20/llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Alignment.h>
#include <string>
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
      auto *returnType = funcTy.returnType()
                             ? getLLVMType(*funcTy.returnType(), false)
                             : llvm::Type::getVoidTy(getLLVMContext());
      auto llvmFuncType =
          llvm::FunctionType::get(returnType, paramTypes, false);
      return llvmFuncType;
    }
    case SemaType::TypeKind::CLS:
      break;
    case SemaType::TypeKind::INST:
      return llvm::PointerType::get(getLLVMContext(), 0);
  }
  return nullptr;
}
llvm::GlobalVariable *CodeGenCtx::createGlobalVariable(const llvm::StringRef name, llvm::Constant *init) {
  module->getOrInsertGlobal(name, init->getType());
  auto variable = module->getNamedGlobal(name);
  variable->setAlignment(llvm::MaybeAlign(4));
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
  auto func =
      llvm::Function::Create(fnType, llvm::GlobalValue::ExternalLinkage, funcSym->getName(), module.get());
  verifyFunction(*func);
  env->bindFunc(funcSym, func);
  return func;
}

void CodeGenCtx::createFunctionBlock(llvm::Function *fn) {
  auto entry = createBasicBlock("entry", fn);
  builder->SetInsertPoint(entry);
}

llvm::BasicBlock *CodeGenCtx::createBasicBlock(const llvm::StringRef bbName, llvm::Function *parentFunc) {
  return llvm::BasicBlock::Create(getLLVMContext(), bbName, parentFunc);
}

// inherit parent class field
size_t CodeGenCtx::getTypeSize(llvm::Type *type) {
  return module->getDataLayout().getTypeAllocSize(type);
}

llvm::Value *CodeGenCtx::createInstance(const string &clsName, const string &varName, Environment::Env env) {
  auto cls = llvm::StructType::getTypeByName(getLLVMContext(), clsName);
  auto instance = mallocInstance(cls, varName);
  // call Constructor
  auto constor = getModule().getFunction(clsName + "_constructor");
  if (constor->arg_size() == 1) {
    getBuilder().CreateCall(constor, {instance});
  }
  return instance;
}

// allocate an object of a given class on the heap
llvm::Value *CodeGenCtx::mallocInstance(llvm::StructType *cls, const std::string &name) {
  auto typeSize = builder->getInt64(getTypeSize(cls));

  llvm::Value *instance = builder->CreateCall(module->getFunction("malloc"), typeSize, name);
  // auto mallocPtr = nullptr;
  // void* -> Point*
  //auto instance = builder->CreatePointerCast(mallocPtr, llvm::PointerType::get(cls, 0));

  // set vtable
  std::string className{cls->getName().data()};
  auto vTableName = className + "_vTable";
  auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);
  auto vTable = module->getNamedGlobal(vTableName);

  builder->CreateStore(vTable, vTableAddr);

  return instance;
}
void CodeGenCtx::buildClassInfo(llvm::StructType *cls, const ClassDecl &node, Environment::Env env) {
  string clsName = node.identifier();
  const auto *clsSym = node.getClassSymbol();
  auto &classInfo = classMap[clsName];
  // member variable need to be added outside of the class
  // add using code like cls.a=1; and visit as well

  // now suppport declare variable in class
  for (auto *method: clsSym->getMethods()) {
    string methodName = clsName + "_" + method->getName();
    method->setName(methodName);
    auto methodSig = buildSignature(method, false, true);
    auto llvmFuncType =
        llvm::FunctionType::get(methodSig.retTy, methodSig.paramTys, false);
    classInfo->methodsMap[methodName] =
        createFunctionProto(method, llvmFuncType, env);
  }
  for (auto *field: clsSym->getFields()) {
    auto fieldType = getLLVMType(*(field->getType()));
    classInfo->fieldsMap[field->getName()] = fieldType;
  }
  buildClassBody(cls);
}
void CodeGenCtx::buildClassBody(llvm::StructType *cls) {
  string clsName = cls->getName().data();
  auto clsInfo = classMap[clsName].get();
  // allocate vtable to set type
  string vTabeleName = clsName + "_vTable";
  // ! we now create virtual table of a class which means we create actual class and its members
  llvm::StructType *vTableType = llvm::StructType::create(getLLVMContext(), vTabeleName);

  // virtual table first has a pointer to itself
  // auto clsField = vec<llvm::Type *>{vTableType->getPointerTo()};
  // 1 => global address space; 0 => generic address space
  auto clsField = vec<llvm::Type *>{llvm::PointerType::get(vTableType, 0)};
  for (const auto &field: clsInfo->fieldsMap) {
    clsField.push_back(field.second);
  }
  cls->setBody(clsField, false);
  // build methods
  buildVTable(cls, clsInfo);
}
void CodeGenCtx::buildVTable(llvm::StructType *cls, ClassInfo *classInfo) {
  string clsName = cls->getName().data();
  string vTableName = clsName + "_vTable";
  auto vTableType = llvm::StructType::getTypeByName(getLLVMContext(), vTableName);

  vec<llvm::Constant *> vTableMethods;
  vec<llvm::Type *> vTableMethodTypes;

  for (const auto &method: classInfo->methodsMap) {
    vTableMethods.push_back(method.second);
    vTableMethodTypes.push_back(method.second->getType());
  }

  vTableType->setBody(vTableMethodTypes);
  auto vTableValue = llvm::ConstantStruct::get(vTableType, vTableMethods);
  // vTableGlobal->setInitializer(vTableValue);
  createGlobalVariable(vTableName, vTableValue);
}
size_t CodeGenCtx::getFieldIndex(llvm::StructType *cls, const std::string &fieldName) {
  auto &fMap = classMap[cls->getName().data()]->fieldsMap;
  auto it = fMap.find(fieldName);
  return std::distance(fMap.begin(), it) + RESERVED_FIELD_COUNT;// +1 because the first one is self
}
size_t CodeGenCtx::getMethodIndex(llvm::StructType *cls, const std::string &methodName) {
  auto mMap = classMap[cls->getName().data()]->methodsMap;
  auto it = mMap.find(methodName);
  return std::distance(mMap.begin(), it);
}
bool CodeGenCtx::isMethod(llvm::StructType *cls, const std::string &name) {
  return false;
}

CodeGenCtx::FuncSignature CodeGenCtx::buildSignature(const FuncSymbol *funcSym, bool isMain, bool isMethod) {
  CodeGenCtx::FuncSignature sig;
  sig.paramTys.clear();

  // build parameter types of function
  if (funcSym) {
    if (isMethod) {
      sig.paramTys.push_back(llvm::PointerType::get(curCls, 0));
    }
    for (const auto &p: funcSym->getParams()) {
      const bool byRef = p->getPass() == Symbol::ParamPass::BY_REF;
      llvm::Type *ty = byRef ? llvm::PointerType::get(getLLVMContext(), 0)
                             : getLLVMType(*p->getType(), true);
      sig.paramTys.push_back(ty);
    }
  }

  if (!isMain) {
    auto *fnSig =
        funcSym ? static_cast<const FuncType *>(funcSym->getType().get())
                : nullptr;
    sig.retTy = (fnSig && fnSig->returnType())
                    ? getLLVMType(*fnSig->returnType())
                    : llvm::Type::getVoidTy(getLLVMContext());
  } else {
    // if this functino is main funciton, we just return 0;
    sig.retTy = llvm::Type::getInt32Ty(getLLVMContext());
    sig.paramTys.clear();
  }

  return sig;
}

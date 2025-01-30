#include "../include/IRGen.hpp"
#include "../include/Expr.hpp"
#include "../include/Logger.hpp"
#include "../include/Stmt.hpp"
#include "../include/Token.hpp"
#include "Object.hpp"
#include <cstdint>
#include <llvm-14/llvm/IR/Constant.h>
#include <llvm-14/llvm/IR/Constants.h>
#include <llvm-14/llvm/IR/DerivedTypes.h>
#include <llvm-14/llvm/IR/Function.h>
#include <llvm-14/llvm/IR/GlobalVariable.h>
#include <llvm-14/llvm/IR/Instructions.h>
#include <llvm-14/llvm/IR/LegacyPassManager.h>
#include <llvm-14/llvm/IR/Type.h>
#include <llvm-14/llvm/IR/Value.h>
#include <llvm-14/llvm/IR/Verifier.h>
#include <llvm-14/llvm/Support/Casting.h>
#include <llvm-14/llvm/Support/TargetSelect.h>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <llvm-14/llvm/Target/TargetMachine.h>
#include <llvm-14/llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm-14/llvm/Transforms/Scalar.h>
#include <llvm-14/llvm/Transforms/Scalar/GVN.h>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

llvm::Value *IRGenerator::lastValue = nullptr;
void IRGenerator::compileAndSave(vector<shared_ptr<Stmt>> &statements) {
    // 1. compile ast
    try {
        compile(statements);
    } catch (std::runtime_error &error) {
        return;
    }

    // 2. print llvm IR
    // module->print(llvm::outs(), nullptr);
    // 3. save module to file
    saveModuleToFile("./output.ll");
}

void IRGenerator::saveModuleToFile(const std::string &fileName) {
    std::error_code errorCode;
    llvm::raw_fd_ostream outLL(fileName, errorCode);
    module->print(outLL, nullptr);
}

void IRGenerator::compile(vector<shared_ptr<Stmt>> &statements) {
    curFn = createFunction("main", llvm::FunctionType::get(builder->getInt32Ty(), false), globalEnv);

    gen(statements);

    // return 0
    builder->CreateRet(builder->getInt32(0));
}

llvm::Value *IRGenerator::gen(vector<shared_ptr<Stmt>> &statements) {
    // generate IR based on the AST (using visitor pattern)
    for (auto stmt: statements) {
        execute(stmt);
    }
    return IRGenerator::lastValue;
}

void IRGenerator::moduleInit() {
    ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("Cat-lang", *ctx);
    builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    varsBuilder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    // optimization
    fpm = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    fpm->add(llvm::createInstructionCombiningPass());
    // Reassociate expressions.
    fpm->add(llvm::createReassociatePass());
    // Eliminate Common SubExpressions.
    fpm->add(llvm::createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    fpm->add(llvm::createCFGSimplificationPass());

    fpm->doInitialization();
    IRGenerator::lastValue = builder->getInt32(0);
}

llvm::Function *IRGenerator::createFunction(const std::string &fnName, llvm::FunctionType *fnType, Env env) {
    // function prototype might already exist in the module
    auto fn = module->getFunction(fnName);
    if (fn == nullptr) {
        fn = createFunctionProto(fnName, fnType, env);
    }
    // create basic block
    createFunctionBlock(fn);
    return fn;
}

llvm::Function *IRGenerator::createFunctionProto(const std::string &fnName, llvm::FunctionType *fnType, Env env) {
    auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, module.get());
    verifyFunction(*fn);

    env->define(fnName, fn);
    return fn;
}

llvm::Value *IRGenerator::allocVar(const std::string &name, llvm::Type *type, Env env) {
    // get entry block
    llvm::BasicBlock &entryBlock = curFn->getEntryBlock();

    // set entry to the beginning of the block
    varsBuilder->SetInsertPoint(&entryBlock, entryBlock.begin());
    // varsBuilder->SetInsertPoint(&curFn->getEntryBlock());

    auto varAlloc = varsBuilder->CreateAlloca(type, 0, name.c_str());

    env->define(name, varAlloc);
    return varAlloc;
}

llvm::GlobalVariable *IRGenerator::createGlobalVariable(const std::string &name, llvm::Constant *init) {
    module->getOrInsertGlobal(name, init->getType());
    auto variable = module->getNamedGlobal(name);
    variable->setConstant(false);
    variable->setInitializer(init);
    return variable;
}

/* create a basic block */
llvm::BasicBlock *IRGenerator::createBB(const std::string &name, llvm::Function *fn = nullptr) {
    return llvm::BasicBlock::Create(*ctx, name, fn);
}

/* create a function block */
void IRGenerator::createFunctionBlock(llvm::Function *fn) {
    auto entry = createBB("entry", fn);
    builder->SetInsertPoint(entry);
}

/* set up external functions like print*/
void IRGenerator::setupExternalFunctions() {
    // void print(string)
    auto bytePtrTy = builder->getInt8PtrTy();                                                              //->getPointerTo();
    module->getOrInsertFunction("printf", llvm::FunctionType::get(builder->getInt32Ty(), bytePtrTy, true));// true means varargs

    module->getOrInsertFunction("GC_malloc", llvm::FunctionType::get(bytePtrTy, builder->getInt64Ty(), false));
}

void IRGenerator::setupGlobalEnvironment() {
    std::map<string, llvm::Value *> globalObjects{
        {"CYLANG_VERSION", builder->getInt32(42)},
    };
    std::map<string, llvm::Value *> globalRecords{};

    for (auto &entry: globalObjects) {
        globalRecords[entry.first] = createGlobalVariable(entry.first, (llvm::Constant *) entry.second);
    }
    globalEnv = std::make_shared<Environment>(nullptr, globalRecords);
}

void IRGenerator::setupTargetTriple() {
    module->setTargetTriple("x86_64-pc-linux-gnu");
}

llvm::Type *IRGenerator::excrateVarType(std::shared_ptr<Expr<Object>> expr) {
    if (std::dynamic_pointer_cast<Literal<Object>>(expr) != nullptr) {
        auto literal = std::dynamic_pointer_cast<Literal<Object>>(expr);
        switch (literal->value.data.index()) {
            case 0:
                // string type
                return builder->getInt8PtrTy();
            case 1:
                // int type
                return builder->getInt32Ty();
            case 2:
                // double type
                return builder->getDoubleTy();
            case 3:
                // bool type
                return builder->getInt1Ty();
        }
    }
    return builder->getInt32Ty();
}

llvm::Type *IRGenerator::excrateTypeByName(const string &typeName, const Token tok, const std::vector<uint64_t> &dims = {}) {

    if (typeName == "int") {
        return builder->getInt32Ty();
    } else if (typeName == "str") {
        return builder->getInt8PtrTy();//i8**
    } else if (typeName == "bool") {
        return builder->getInt1Ty();
    } else if (typeName == "double") {
        return builder->getDoubleTy();
    } else if (typeName.find("list<") == 0) {// nested list, like list<list<int>>
        string innerType = typeName.substr(5, typeName.size() - 6);

        uint64_t currentDim = dims[0];
        std::vector<uint64_t> remainingDims(dims.begin() + 1, dims.end());

        //recursively retrive type util base
        llvm::Type *innerTypePtr = excrateTypeByName(innerType, tok, remainingDims);

        // consturct list type of current level
        return llvm::ArrayType::get(innerTypePtr, currentDim);
    } else if (typeName == "") {
        return builder->getInt32Ty();
    }
    auto varTy = classMap_[typeName].cls;
    if (varTy == nullptr) {
        Error::addError(tok, "[CyLang]: Undefined type: " + typeName);
        throw std::runtime_error("Undefined type");
    }
    return varTy->getPointerTo();
}

// related to class helper function
llvm::StructType *IRGenerator::getClassByName(const std::string &name) {
    return llvm::StructType::getTypeByName(*ctx, name);
}

// create instance
llvm::Value *IRGenerator::createInstance(shared_ptr<Call<Object>> expr, const std::string &varName = "") {
    string className = "";
    if (expr->callee->type == ExprType::Variable) {
        className = std::dynamic_pointer_cast<Variable<Object>>(expr->callee)->name.lexeme;
    }

    auto cls = getClassByName(className);

    if (cls == nullptr) {
        // Error::ErrorLogMessage() << "[EvaVM]: Undefined class: " << cls;
        Error::addError(expr->paren, "[CyLang]: Undefined class:" + className);
        throw std::runtime_error("Undefined class");
    }
    auto instance = mallocInstance(cls, varName);

    // call constructor
    auto ctor = module->getFunction(className + "_init");

    std::vector<llvm::Value *> args{instance};
    // clang-format off
    // check if the number of arguments is correct
    if (ctor->arg_size() - 1 != expr->arguments.size()) {
        Error::addError(expr->paren, "[CyLang]: No matched constructor, expect " + 
                                    std::to_string(ctor->arg_size() - 1) + 
                                    " parameters, but got " + 
                                    std::to_string(expr->arguments.size()));
        throw std::runtime_error("No matched constructor");
    }
    // clang-format on
    for (auto arg: expr->arguments) {
        args.push_back(evaluate(arg));
    }

    builder->CreateCall(ctor, args);
    return instance;
}

// allocate an object of a given class on the heap
llvm::Value *IRGenerator::mallocInstance(llvm::StructType *cls, const std::string &name) {
    auto typeSize = builder->getInt64(getTypeSize(cls));

    auto mallocPtr = builder->CreateCall(module->getFunction("GC_malloc"), typeSize, name);

    // void* -> Point*
    auto instance = builder->CreatePointerCast(mallocPtr, cls->getPointerTo());

    // set vtable
    std::string className{cls->getName().data()};
    auto vTableName = className + "_vTable";
    auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);
    auto vTable = module->getNamedGlobal(vTableName);

    builder->CreateStore(vTable, vTableAddr);

    return instance;
}

llvm::Value *IRGenerator::createList(shared_ptr<List<Object>> expr, string ListType = "list<int>") {
    // derive the dimension of list
    vector<uint64_t> dimensions{};
    auto currentExpr = expr;
    while (true) {
        dimensions.push_back(currentExpr->items.size());
        if (currentExpr->items.empty() ||
            currentExpr->items[0]->type != ExprType::List) break;
        currentExpr = std::dynamic_pointer_cast<List<Object>>(currentExpr->items[0]);
    }
    // nested list type using calculated dimension; [2x[3xi32]]
    llvm::Type *finalType = excrateTypeByName(ListType, expr->opening_bracket, dimensions);

    // memory allocation
    auto listAlloc = builder->CreateAlloca(finalType, nullptr, "list");

    // 递归初始化函数
    std::function<void(llvm::Value *, shared_ptr<List<Object>>, size_t)> initArray;
    initArray = [&](llvm::Value *basePtr, shared_ptr<List<Object>> currList, size_t dim) {
        for (size_t i = 0; i < currList->items.size(); ++i) {
            llvm::Value *indices[] = {builder->getInt32(0), builder->getInt32(i)};
            auto elemPtr = builder->CreateInBoundsGEP(
                basePtr->getType()->getPointerElementType(),
                basePtr,
                indices
            );

            if (dim < dimensions.size() - 1) {// process nested dimension recursively
                auto subList = std::dynamic_pointer_cast<List<Object>>(currList->items[i]);
                // clang-format off
                if (!subList || subList->items.size() != dimensions[dim + 1]) {
                    Error::addError(expr->opening_bracket, "Sub-list size mismatch. Expected " 
                                                                            + std::to_string(dimensions[dim + 1]) 
                                                                            + ", got " + std::to_string(subList->items.size()));
                    throw std::runtime_error("Dimension mismatch");
                }
                // clang-format on
                initArray(elemPtr, subList, dim + 1);
            } else {
                auto val = evaluate(currList->items[i]);
                builder->CreateStore(val, elemPtr);
            }
        }
    };

    initArray(listAlloc, expr, 0);
    return listAlloc;
}


// return size of type in bytes
size_t IRGenerator::getTypeSize(llvm::Type *type) {
    return module->getDataLayout().getTypeAllocSize(type);
}

void IRGenerator::inheritClass(llvm::StructType *cls, llvm::StructType *parent) {
    auto parentClassInfo = &classMap_[parent->getName().data()];

    // inherit the field and method
    classMap_[cls->getName().data()] = {cls, parent, parentClassInfo->fieldsMap, parentClassInfo->methodsMap};
}

void IRGenerator::buildClassInfo(llvm::StructType *cls, const Class &stmt, Env env) {
    auto className = stmt.name.lexeme;
    auto classInfo = &classMap_[className];
    // member variable need to be added outside of the class
    // add using code like cls.a=1; and visit as well

    // now suppport declare variable in class
    for (auto mem_met: stmt.body->statements) {
        if (mem_met->type == StmtType::Function) {
            auto method = std::dynamic_pointer_cast<Function>(mem_met);
            auto methodName = method->functionName.lexeme;
            auto fnName = className + "_" + methodName;
            // add 'this' as the first argument
            // method->params.insert(method->params.begin(), {Token(THIS, "this", Object::make_nil_obj(), 0), ""});
            classInfo->methodsMap[methodName] = createFunctionProto(fnName, excrateFunType(method), env);
        } else if (mem_met->type == StmtType::Var) {
            auto member = std::dynamic_pointer_cast<Var>(mem_met);
            auto fieldName = member->name.lexeme;
            auto fieldType = excrateTypeByName(member->typeName, stmt.name);

            classInfo->fieldsMap[fieldName] = fieldType;
        }
    }

    // create field
    buildClassBody(cls);
}

// build class body
void IRGenerator::buildClassBody(llvm::StructType *cls) {
    std::string className{cls->getName().data()};

    auto classInfo = &classMap_[className];

    // allocate the vtable to set its type in the body
    // the table itself is populated later in buildVTable
    auto vTableName = className + "_vTable";
    auto vTableType = llvm::StructType::create(*ctx, vTableName);

    auto clsFields = std::vector<llvm::Type *>{vTableType->getPointerTo()};
    for (const auto &field: classInfo->fieldsMap) {
        clsFields.push_back(field.second);
    }

    cls->setBody(clsFields, false);

    // methods
    buildVTable(cls, classInfo);
}

void IRGenerator::buildVTable(llvm::StructType *cls, ClassInfo *classInfo) {
    std::string className{cls->getName().data()};
    auto vTableName = className + "_vTable";
    auto vTableType = llvm::StructType::getTypeByName(*ctx, vTableName);

    std::vector<llvm::Constant *> vTableMethods;
    std::vector<llvm::Type *> vTableMethodTypes;

    for (const auto &methodInfo: classInfo->methodsMap) {
        vTableMethods.push_back(methodInfo.second);
        vTableMethodTypes.push_back(methodInfo.second->getType());
    }

    vTableType->setBody(vTableMethodTypes);
    auto vTableValue = llvm::ConstantStruct::get(vTableType, vTableMethods);
    createGlobalVariable(vTableName, vTableValue);
}

size_t IRGenerator::getFieldIndex(llvm::StructType *cls, const std::string &fieldName) {
    auto fields = &classMap_[cls->getName().data()].fieldsMap;
    auto it = fields->find(fieldName);
    return std::distance(fields->begin(), it) + RESERVED_FIELD_COUNT;
}

size_t IRGenerator::getMethodIndex(llvm::StructType *cls, const std::string &methodName) {
    auto methods = &classMap_[cls->getName().data()].methodsMap;
    auto it = methods->find(methodName);
    return std::distance(methods->begin(), it);
}

llvm::FunctionType *IRGenerator::excrateFunType(shared_ptr<Function> stmt) {
    auto params = stmt->params;
    auto returnType = stmt->returnTypeName.type == NONE ? builder->getInt32Ty()
                                                        : excrateTypeByName(stmt->returnTypeName.lexeme, stmt->returnTypeName);
    // auto returnType = builder->getInt32Ty();
    std::vector<llvm::Type *> paramTypes{};
    for (auto &param: params) {
        auto paramType = excrateTypeByName(param.second, param.first);
        // auto paramType = builder->getInt32Ty();
        auto paramName = param.first.lexeme;
        paramTypes.push_back(paramName == "self" ? (llvm::Type *) cls->getPointerTo() : paramType);
    }
    return llvm::FunctionType::get(returnType, paramTypes, false);
    // return llvm::FunctionType::get(returnType, paramTypes, false);
}


bool IRGenerator::isMethod(llvm::StructType *cls, const std::string &name) {
    auto methods = &classMap_[cls->getName().data()].methodsMap;
    auto parent = classMap_[cls->getName().data()].parent;
    auto it = methods->find(name);
    if (it != methods->end()) {
        return true;
    } else if (parent != nullptr) {
        return isMethod(parent, name);
    }
    return false;// Not found in the vtable
}

bool IRGenerator::isInstanceArgument(shared_ptr<Expr<Object>> expr) {
    // Check if the expression is a variable
    auto varExpr = std::dynamic_pointer_cast<Variable<Object>>(expr);
    if (!varExpr) {
        return false;// Not a variable, cannot be an instance
    }

    auto instance = environment->lookup(varExpr->name.lexeme);
    if (!instance) {
        return false;
    }
    // Check if the variable represents an allocated instance (e.g., a class or struct instance)
    return llvm::isa<llvm::AllocaInst>(instance);
}

void IRGenerator::executeBlock(vector<shared_ptr<Stmt>> statements, Env env) {
    EnvironmentGuard env_guard{*this, env};
    for (auto stmt: statements) {
        execute(stmt);
    }
}

void IRGenerator::execute(shared_ptr<Stmt> stmt) {
    stmt->accept(shared_from_this());
}

llvm::Value *IRGenerator::evaluate(shared_ptr<Expr<Object>> expr) {
    // ir->print(llvm::outs());
    return expr->accept(shared_from_this()).llvmValue;
}

Object IRGenerator::visitLiteralExpr(shared_ptr<Literal<Object>> expr) {
    //llvm to generate IR for literal
    llvm::Value *val = nullptr;
    switch (expr->value.data.index()) {
        case 0: {
            // string type
            std::string str_data = std::get<std::string>(expr->value.data);
            // handle the \n
            auto re = std::regex("\\\\n");
            str_data = std::regex_replace(str_data, re, "\n");
            val = builder->CreateGlobalStringPtr(str_data);// i8*
            return Object::make_llvmval_obj(val);
        }
        case 1: {
            // int type
            int i32_data = std::get<int>(expr->value.data);
            val = builder->getInt32(i32_data);
            return Object::make_llvmval_obj(val);
        }
        case 2: {
            // double type
            double dnumber_data = std::get<double>(expr->value.data);
            val = llvm::ConstantFP::get(builder->getDoubleTy(), dnumber_data);
            return Object::make_llvmval_obj(val);
        }
        case 3: {
            // bool type
            bool b_data = std::get<bool>(expr->value.data);
            val = builder->getInt1(b_data);
            return Object::make_llvmval_obj(val);
        }
    }


    return Object::make_llvmval_obj(builder->getInt32(0));
}

Object IRGenerator::visitAssignExpr(shared_ptr<Assign<Object>> expr) {
    //llvm to generate IR for assignment
    auto name = expr->name;
    auto value = evaluate(expr->value);
    // variable
    auto varBinding = this->environment->lookup(name);
    if (!varBinding) {
        Error::addError(name, "[CyLang]: Undefined variable: " + name.lexeme);
        throw std::runtime_error("Undefined variable");
    }
    if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(varBinding->getType());
        ptrType && ptrType->getPointerElementType() != value->getType()) {
        Error::addError(name, "[CyLang]: Target type mismatch");
        throw std::runtime_error("Type mismatch");
    }
    builder->CreateStore(value, varBinding);
    return Object::make_llvmval_obj(value);
}

Object IRGenerator::visitBinaryExpr(shared_ptr<Binary<Object>> expr) {
    //llvm to generate IR for binary
    auto left = evaluate(expr->left);
    auto leftOperatorType = left->getType();
    auto right = evaluate(expr->right);
    auto rightOperatorType = right->getType();
    // check if the type of left and right are the same
    if (leftOperatorType != rightOperatorType) {
        Error::addError(expr->operation, "[CyLang]: Binary operarion types mismatch");
        throw std::runtime_error("Type mismatch");
    }
    if (expr->operation.lexeme == "+") {
        // check if operator is int or double
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateAdd(left, right, "tmpiadd"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFAdd(left, right, "tmpfadd"));
        }
    } else if (expr->operation.lexeme == "-") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateSub(left, right, "tmpisub"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFSub(left, right, "tmpfsub"));
        }
    } else if (expr->operation.lexeme == "*") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateMul(left, right, "tmpimul"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFMul(left, right, "tmpfmul"));
        }
    } else if (expr->operation.lexeme == "/") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateSDiv(left, right, "tmpidiv"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFDiv(left, right, "tmpfdiv"));
        }
    }
    // Unsigned comparison
    else if (expr->operation.lexeme == ">") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpSGT(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpOGT(left, right, "tmpfcmp"));
        }
    } else if (expr->operation.lexeme == "<") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpSLT(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpOLT(left, right, "tmpfcmp"));
        }
    } else if (expr->operation.lexeme == "==") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpEQ(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpOEQ(left, right, "tmpfcmp"));
        }
    } else if (expr->operation.lexeme == "!=") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpNE(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpONE(left, right, "tmpfcmp"));
        }
    } else if (expr->operation.lexeme == ">=") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpSGE(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpOGE(left, right, "tmpfcmp"));
        }
    } else if (expr->operation.lexeme == "<=") {
        if (leftOperatorType->isIntegerTy()) {
            return Object::make_llvmval_obj(builder->CreateICmpSLE(left, right, "tmpicmp"));
        } else if (leftOperatorType->isDoubleTy()) {
            return Object::make_llvmval_obj(builder->CreateFCmpOLE(left, right, "tmpfcmp"));
        }
    }

    return Object::make_llvmval_obj(builder->getInt32(0));
}

Object IRGenerator::visitGroupingExpr(shared_ptr<Grouping<Object>> expr) {
    // llvm to generate IR for grouping
    auto value = evaluate(expr->expression);
    return Object::make_llvmval_obj(value);
}

Object IRGenerator::visitUnaryExpr(shared_ptr<Unary<Object>> expr) {
    //llvm to generate IR for unary
    auto right = evaluate(expr->right);
    if (expr->operation.lexeme == "-") {
        lastValue = builder->CreateNeg(right);
    } else if (expr->operation.lexeme == "!") {
        lastValue = builder->CreateNot(right);
    }
    return Object::make_llvmval_obj(lastValue);
}

Object IRGenerator::visitVariableExpr(shared_ptr<Variable<Object>> expr) {
    // use the declared variable
    string varName = expr->name.lexeme;
    Token name = expr->name;
    auto value = environment->lookup(name);
    //local variable
    if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value)) {
        auto var = builder->CreateLoad(localVar->getAllocatedType(), localVar, varName.c_str());
        return Object::make_llvmval_obj(var);
    }

    // global variable
    else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        auto var = builder->CreateLoad(globalVar->getInitializer()->getType(), globalVar, varName.c_str());
        return Object::make_llvmval_obj(var);
    } else {
        return Object::make_llvmval_obj(value);
    }
    // return Object::make_llvmval_obj(builder->getInt32(0));
}

Object IRGenerator::visitLogicalExpr(shared_ptr<Logical<Object>> expr) {
    // Logical
    if (expr->operation.lexeme == "and") {
        GEN_BINARY_OP(CreateAnd, "tmpand");
    } else if (expr->operation.lexeme == "or") {
        GEN_BINARY_OP(CreateOr, "tmpor");
    }
    return Object::make_llvmval_obj(builder->getInt32(0));
}

// Object VM::visitIncrementExpr(shared_ptr<Increment<Object>> expr) {
//     return Object::make_llvmval_obj(builder->getInt32(0));
// }

// Object VM::visitDecrementExpr(shared_ptr<Decrement<Object>> expr) {
//     return Object::make_llvmval_obj(builder->getInt32(0));
// }

Object IRGenerator::visitListExpr(shared_ptr<List<Object>> expr) {
    auto list = createList(expr);
    return Object::make_llvmval_obj(list);
}

Object IRGenerator::visitSubscriptExpr(shared_ptr<Subscript<Object>> expr) {
    auto listName = expr->identifier.lexeme;
    auto list = environment->lookup(listName);// list<str> = ["s", "t", "r"]=>[size * i8*]*
    // list->getType()->dump();                  // [3 * i8*]*
    auto index = evaluate(expr->index);
    auto listType = list->getType()->getPointerElementType();
    auto elementTy = listType->getArrayElementType();
    // elementTy->dump();// i8*
    auto ptr = builder->CreateInBoundsGEP(listType, list, {builder->getInt32(0), index});

    if (expr->value != nullptr) {// assignment(lvalue)
        auto value = evaluate(expr->value);
        lastValue = builder->CreateStore(value, ptr);
    } else {// rvalue
        lastValue = builder->CreateLoad(elementTy, ptr);
    }


    return Object::make_llvmval_obj(lastValue);
}

Object IRGenerator::visitCallExpr(shared_ptr<Call<Object>> expr) {
    auto callee = evaluate(expr->callee);


    // check if the callee is the method
    if (auto loadedMethod = llvm::dyn_cast<llvm::LoadInst>(callee)) {
        callee = loadedMethod->getPointerOperand();
        auto fnType = (llvm::FunctionType *) (callee->getType()
                                                  ->getContainedType(0)
                                                  ->getContainedType(0));
        std::vector<llvm::Value *> args{};
        // by default, the first argument is the instance
        if (expr->arguments.empty() || !isInstanceArgument(expr->arguments[0])) {
            llvm::Type *targetType = fnType->getParamType(0);
            auto bitCastArgVal = builder->CreateBitCast(callee, targetType);
            args.push_back(bitCastArgVal);
        }

        // check if the number of method arguments is correct
        // clang-format off
        if (fnType->getNumParams() - 1 != expr->arguments.size()) {
            Error::addError(expr->paren, "[CyLang]: No matched method, expect " + 
                                                        std::to_string(fnType->getNumParams() - 1) +
                                                        " parameters, but got " + std::to_string(expr->arguments.size()));
            throw std::runtime_error("No matched method");
        }
        // clang-format on
        for (int i = 0; i < expr->arguments.size(); i++) {
            auto argValue = evaluate(expr->arguments[i]);
            // Need to cast parameter type to support polymorphism(sub class)
            // we should be able to pass Point3D instance for the type
            // of the parent class Point
            auto paramType = fnType->getParamType(i + 1);
            if (argValue->getType() != paramType) {
                auto bitCastArgVal = builder->CreateBitCast(argValue, paramType);
                args.push_back(bitCastArgVal);
            } else {
                args.push_back(argValue);
            }
        }
        auto val = builder->CreateCall(fnType, loadedMethod, args);
        return Object::make_llvmval_obj(val);
    } else {

        std::vector<llvm::Value *> args{};
        auto argIndex = 0;
        auto calleeType = callee->getType()->getContainedType(0);
        // if it is closure
        if (calleeType->isStructTy()) {
            auto cls = (llvm::StructType *) calleeType;
            std::string className{cls->getName().data()};

            // push self as the first argument
            args.push_back(callee);
            argIndex++;

            // TODO support polymorphism (inheritance)
            callee = module->getFunction(className + "_" + "__call__");// 3 _
        }

        auto fn = (llvm::Function *) callee;

        // check if the number of arguments is correct
        // clang-format off
        if (fn->arg_size()-args.size() != expr->arguments.size()) {
            Error::addError(expr->paren, "[CyLang]: No matched function, expect " + 
                                        std::to_string(fn->arg_size()-args.size()) + 
                                        " parameters, but got " + 
                                        std::to_string(expr->arguments.size()));
            throw std::runtime_error("No matched function");
        }
        // clang-format on

        for (auto arg: expr->arguments) {
            auto argValue = evaluate(arg);
            auto paramType = fn->getArg(argIndex)->getType();
            auto bitCastArgVal = builder->CreateBitCast(argValue, paramType);
            argIndex++;
            args.push_back(bitCastArgVal);
        }


        auto val = builder->CreateCall(fn, args);
        return Object::make_llvmval_obj(val);
    }
}

Object IRGenerator::visitGetExpr(shared_ptr<Get<Object>> expr) {
    // instance
    auto instance = evaluate(expr->object);
    auto fieldName = expr->name.lexeme;
    llvm::StructType *cls = (llvm::StructType *) (instance->getType()->getContainedType(0));
    if (isMethod(cls, fieldName)) {
        // Load the method from the vtable
        llvm::Value *vTable = nullptr;
        llvm::StructType *vTableType = nullptr;

        // Access the vtable
        auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX, "vtable_ptr");
        vTable = builder->CreateLoad(cls->getElementType(VTABLE_INDEX), vTableAddr, "vtable");
        vTableType = (llvm::StructType *) (vTable->getType()->getContainedType(0));

        auto methodIdx = getMethodIndex(cls, fieldName);
        auto methodType = (llvm::FunctionType *) vTableType->getElementType(methodIdx);

        auto methodAddr = builder->CreateStructGEP(vTableType, vTable, methodIdx, "method_ptr");
        auto methodValue = builder->CreateLoad(methodType, methodAddr, "method");

        return Object::make_llvmval_obj(methodValue);
    } else {// function
        auto ptrName = std::string("p") + fieldName;
        auto fieldIdx = getFieldIndex(cls, fieldName);
        auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);

        auto fieldType = cls->getElementType(fieldIdx);

        auto value = builder->CreateLoad(fieldType, address, fieldName);
        return Object::make_llvmval_obj(value);
    }
}

Object IRGenerator::visitSetExpr(shared_ptr<Set<Object>> expr) {
    auto instance = evaluate(expr->object);// %Point**
    auto fieldName = expr->name.lexeme;
    auto ptrName = std::string("p") + fieldName;

    auto cls = (llvm::StructType *) (instance->getType()->getContainedType(0));
    auto fieldIdx = getFieldIndex(cls, fieldName);

    auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);
    auto value = evaluate(expr->value);
    builder->CreateStore(value, address);
    return Object::make_llvmval_obj(value);
}

Object IRGenerator::visitSelfExpr(shared_ptr<Self<Object>> expr) {
    auto selfValue = environment->lookup(expr->keyword.lexeme);
    if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(selfValue)) {
        lastValue = builder->CreateLoad(localVar->getAllocatedType(), localVar, expr->keyword.lexeme.c_str());
    } else {
        lastValue = selfValue;
    }
    return Object::make_llvmval_obj(lastValue);
}

Object IRGenerator::visitSuperExpr(shared_ptr<Super<Object>> expr) {
    // (method (super <class>) <name>)
    // TODO
    string className = expr->className.lexeme;// current class name
    auto methodName = expr->method.lexeme;
    return Object::make_llvmval_obj(builder->getInt32(0));
}

void IRGenerator::visitExpressionStmt(const Expression &stmt) {
    lastValue = evaluate(stmt.expression);
    Values.push_back(lastValue);
}

void IRGenerator::visitPrintStmt(const Print &stmt) {
    auto expr = std::dynamic_pointer_cast<Call<Object>>(stmt.expression);
    auto printFn = module->getFunction("printf");
    // auto str = builder->CreateGlobalStringPtr("Hello, World!");
    std::vector<llvm::Value *> args;
    for (auto arg: expr->arguments) {
        args.push_back(evaluate(arg));
    }

    lastValue = builder->CreateCall(printFn, args);
    Values.push_back(lastValue);
}

void IRGenerator::visitVarStmt(const Var &stmt) {
    // special case for class fields,
    // which are already defined in class info allocation
    // this case is for defining variables in class
    if (cls != nullptr) {
        lastValue = builder->getInt32(0);
        return;
    }
    auto varName = stmt.name.lexeme;
    if (stmt.initializer != nullptr) {

        // we need to check if the variable is a class
        // so that we can allocate the memory for it i.e. define it in the environment
        // ! error
        if (stmt.initializer->type == ExprType::Call &&
            std::dynamic_pointer_cast<Call<Object>>(stmt.initializer)->callee->type == ExprType::Variable &&
            getClassByName(std::dynamic_pointer_cast<Variable<Object>>(
                               std::dynamic_pointer_cast<Call<Object>>(stmt.initializer)->callee
            )
                               ->name.lexeme) != nullptr) {

            auto call = std::dynamic_pointer_cast<Call<Object>>(stmt.initializer);
            auto className = std::dynamic_pointer_cast<Variable<Object>>(call->callee)->name.lexeme;
            auto instance = createInstance(call, varName);
            lastValue = environment->define(varName, instance);
            // evaluate(stmt.initializer);
            Values.push_back(lastValue);
            return;
        } else if (stmt.initializer->type == ExprType::List) {
            // string elemType = stmt.typeName.substr(5, stmt.typeName.size() - 6);// list<T> => T
            auto list = createList(std::dynamic_pointer_cast<List<Object>>(stmt.initializer), stmt.typeName);
            lastValue = environment->define(varName, list);
            Values.push_back(lastValue);
            return;
        }
        auto init = evaluate(stmt.initializer);
        llvm::Type *varTy = nullptr;
        vector<uint64_t> dimensions{};
        // check if init is array
        if (auto arrayType = llvm::dyn_cast<llvm::ArrayType>(init->getType())) {

            auto array = llvm::dyn_cast<llvm::ConstantArray>(init);
            // cal array dimension
            while (arrayType) {
                dimensions.push_back(arrayType->getNumElements());
                arrayType = llvm::dyn_cast<llvm::ArrayType>(arrayType->getElementType());
            }
            varTy = excrateTypeByName(stmt.typeName, stmt.name, dimensions);
        } else {
            varTy = excrateTypeByName(stmt.typeName, stmt.name);
        }

        if (varTy != init->getType()) {
            Error::addError(stmt.name, "[CyLang]: Variable decalaration type mismatch");
            throw std::runtime_error("Type mismatch");
        }
        auto varBinding = allocVar(varName, varTy, environment);
        lastValue = builder->CreateStore(init, varBinding);
        Values.push_back(lastValue);
        return;
        // lastValue = createGlobalVariable(varName, (llvm::Constant *) init)->getInitializer();
    } else {
        // global variable without initializer to be assigned 0
        lastValue = createGlobalVariable(varName, builder->getInt32(0))->getInitializer();
        Values.push_back(lastValue);
        return;
    }
}

void IRGenerator::visitBlockStmt(const Block &stmt) {
    shared_ptr<Environment> env = std::make_shared<Environment>(environment, std::map<std::string, llvm::Value *>{});
    executeBlock(stmt.statements, env);
}

void IRGenerator::visitClassStmt(const Class &stmt) {
    auto className = stmt.name.lexeme;
    auto parent = stmt.superclass == nullptr ? nullptr : getClassByName(stmt.superclass->name.lexeme);

    // compile class
    cls = llvm::StructType::create(*ctx, className);
    // super class data always sit at the beginning
    if (parent != nullptr) {
        inheritClass(cls, parent);
    } else {
        // add class info
        classMap_[className] = {cls, parent, /*field*/ {}, /*method*/ {}};
    }
    // populate the class with fields and methods
    buildClassInfo(cls, stmt, environment);

    // compile the class body
    for (auto mem_met: stmt.body->statements) {
        if (mem_met->type == StmtType::Function) {
            auto method = std::dynamic_pointer_cast<Function>(mem_met);
            method->functionName.lexeme = className + "_" + method->functionName.lexeme;
            // rename the function to include class name
            execute(method);


        } else if (mem_met->type == StmtType::Var) {
            auto member = std::dynamic_pointer_cast<Var>(mem_met);
            execute(member);
        }
    }


    // reset class after compilation, so normal function does not pick the class name prefix
    cls = nullptr;

    lastValue = builder->getInt32(0);
}

void IRGenerator::visitIfStmt(const If &stmt) {
    auto conditon = evaluate(stmt.main_branch.condition);

    auto thenBlock = createBB("then", curFn);
    // else, if-end block to handle nested if expression
    auto elseBlock = createBB("else");
    auto ifEndBlock = createBB("ifend");

    // conditon branch
    builder->CreateCondBr(conditon, thenBlock, elseBlock);
    // then branch
    builder->SetInsertPoint(thenBlock);
    execute(stmt.main_branch.statement);
    auto thenRes = lastValue;
    builder->CreateBr(ifEndBlock);
    // restore block to handle nested if expression
    // which is needed for phi instruction
    thenBlock = builder->GetInsertBlock();
    // else branch
    // append block to function
    // elseBlock->insertInto(curFn);// in llvm 17
    curFn->getBasicBlockList().push_back(elseBlock);// in llvm 14
    builder->SetInsertPoint(elseBlock);
    llvm::Value *elseRes = lastValue;
    if (stmt.else_branch != nullptr) {
        execute(stmt.else_branch);
        elseRes = lastValue;
    } else {
        // If there's no else block, create a branch from elseBlock to ifEndBlock directly
        // and set the default elseRes value.
        elseRes = builder->getInt32(0);// Set default value for elseRes
    }

    builder->CreateBr(ifEndBlock);
    // same for else block
    elseBlock = builder->GetInsertBlock();
    // endif
    // ifEndBlock->insertInto(fn);
    curFn->getBasicBlockList().push_back(ifEndBlock);
    builder->SetInsertPoint(ifEndBlock);

    // result of if expression is phi
    auto phi = builder->CreatePHI(thenRes->getType(), 2, "tmpif");
    phi->addIncoming(thenRes, thenBlock);
    phi->addIncoming(elseRes, elseBlock);
    lastValue = phi;
}
void IRGenerator::visitWhileStmt(const While &stmt) {
    // condition
    auto condBlock = createBB("cond", curFn);
    builder->CreateBr(condBlock);

    // Body:while end loop
    auto bodyBlock = createBB("body");
    auto loopEndBlock = createBB("loopend");

    // compile while
    builder->SetInsertPoint(condBlock);
    auto condition = evaluate(stmt.condition);

    // condition branch
    builder->CreateCondBr(condition, bodyBlock, loopEndBlock);

    // body
    curFn->getBasicBlockList().push_back(bodyBlock);
    builder->SetInsertPoint(bodyBlock);
    execute(stmt.body);

    // jump to condition block unconditionally
    builder->CreateBr(condBlock);

    // end while
    curFn->getBasicBlockList().push_back(loopEndBlock);
    builder->SetInsertPoint(loopEndBlock);
    lastValue = builder->getInt32(0);
}

void IRGenerator::visitFunctionStmt(shared_ptr<Function> stmt) {
    auto fnName = stmt->functionName.lexeme;
    auto params = stmt->params;
    auto funBody = stmt->body;

    // save current function
    auto prevFn = curFn;
    auto prevBlock = builder->GetInsertBlock();
    auto prevEnv = environment;
    // override fn to compile body
    auto newFn = createFunction(fnName, excrateFunType(stmt), environment);
    curFn = newFn;

    // set parameter name
    auto idx = 0;
    environment = std::make_shared<Environment>(environment, std::map<std::string, llvm::Value *>{});

    for (auto &arg: curFn->args()) {
        auto param = params[idx++];
        auto argName = param.first.lexeme;
        arg.setName(argName);

        // allocate a local variable prr argument to make sure arguments mutable
        auto argBinding = allocVar(argName, arg.getType(), environment);
        builder->CreateStore(&arg, argBinding);
    }

    gen(funBody);
    // builder->CreateRet(lastValue);

    // restore previous env after compiling
    builder->SetInsertPoint(prevBlock);
    curFn = prevFn;
    environment = prevEnv;

    // Validate the generated code, checking for consistency.
    verifyFunction(*newFn);
    // Run the optimizer on the function.
    fpm->run(*newFn);

    lastValue = newFn;
}

void IRGenerator::visitReturnStmt(const Return &stmt) {
    auto returnVal = evaluate(stmt.value);
    builder->CreateRet(returnVal);
}

void IRGenerator::visitBreakStmt(const Break &stmt) {}

void IRGenerator::visitContinueStmt(const Continue &stmt) {}

IRGenerator::EnvironmentGuard::EnvironmentGuard(
    IRGenerator &ir_generator, std::shared_ptr<Environment> enclosing_env
)
    : ir_generator{ir_generator}, previous_env{ir_generator.environment} {
    ir_generator.environment = std::move(enclosing_env);
}

IRGenerator::EnvironmentGuard::~EnvironmentGuard() {
    ir_generator.environment = std::move(previous_env);
}

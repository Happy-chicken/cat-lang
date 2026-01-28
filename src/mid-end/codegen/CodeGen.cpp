#include "CodeGen.hpp"
#include "AST.hpp"
#include "CodeGenCtx.hpp"
#include "Diagnostics.hpp"
#include "Environment.hpp"
#include "SemaType.hpp"
#include "SemanticCtx.hpp"
#include "Symbol.hpp"
#include "Token.hpp"
#include "Types.hpp"
#include <cstddef>
#include <llvm-20/llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm-20/llvm/Support/Casting.h>
#include <llvm-20/llvm/Support/TypeName.h>
#include <llvm-20/llvm/TargetParser/Triple.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>

llvm::Value *CodeGen::lastValue = nullptr;

// ------------------------------------------------------------------------------------------------------
// some helper function for Structures and Functions
// ------------------------------------------------------------------------------------------------------

// factory function to build function signature from FuncSymbol

// ------------------------------------------------------------------------------------------------------
CodeGen::CodeGen(CodeGenCtx &ctx) : ctx(ctx) { setupGlobalEnvironment(); }

void CodeGen::visit(Program &node) {
    for (const auto &def: node.getDefs()) {
        def->accept(*this);
    }
}
// ?no ir
void CodeGen::visit(Type &node) {}
void CodeGen::visit(FuncParameterType &node) {}
void CodeGen::visit(Header &node) {}
void CodeGen::visit(FuncParameterDef &node) {}
void CodeGen::visit(VarDef &node) {
    if (node.isField() && ctx.curCls != nullptr) {
        return;
    }
    auto syms = node.symbols();
    auto type = node.declaredType();
    const auto &initExprOpt = node.initExpr();

    // if variable is a instance
    if (type->getName().has_value() && type->data_type() == DataType::DataType::MAY_INSTANCE) {
        for (auto *sym: syms) {
            auto cls = llvm::StructType::getTypeByName(ctx.getLLVMContext(), type->getName().value());
            auto instance = ctx.createInstance(cls->getName().data(), sym->getName(), currentEnv);
            currentEnv->bind(sym, instance);
            lastValue = instance;
            // auto ptrType = llvm::PointerType::get(ctx.getLLVMContext(), 0);
            // auto varAlloca = ctx.getBuilder().CreateAlloca(
            //     ptrType,
            //     nullptr,
            //     "inst_" + sym->getName()
            // );

            // if (initExprOpt.has_value()) {
            //     initExprOpt.value()->accept(*this);
            //     llvm::Value *instance = lastValue;
            //     ctx.getBuilder().CreateStore(instance, varAlloca);
            // }
            // currentEnv->bind(sym, varAlloca);
            // lastValue = varAlloca;
        }
        return;
    }
    // if there is no initializer
    if (!initExprOpt.has_value()) {
        for (auto *sym: syms) {
            string name = sym->getName();
            llvm::Type *llvmType = ctx.getLLVMType(*sym->getType());
            auto varAddr = ctx.createLocalVariable(sym, llvmType, currentEnv);
            lastValue = varAddr;
        }
        //lastValue = nullptr;
        return;
    }
    // there is initializer
    const auto &initExpr = initExprOpt.value();
    initExpr->accept(*this);
    // for each symbol, allocate variable and store the initialized value
    for (auto *sym: syms) {
        string name = sym->getName();
        llvm::Type *llvmType = ctx.getLLVMType(*sym->getType());
        auto varAddr = ctx.createLocalVariable(sym, llvmType, currentEnv);
        ctx.getBuilder().CreateStore(lastValue, varAddr);
    }
    lastValue = nullptr;
}

void CodeGen::visit(FuncDecl &node) {
    /* Resolve Function Symbol & Header */
    auto *header = node.funcHeader();
    auto *funcSym = header ? header->symbol() : nullptr;
    if (!funcSym) {
        lastValue = nullptr;
        return;
    }

    auto sig = ctx.buildSignature(funcSym);
    ensureLLVMFunction(funcSym, sig);
    lastValue = nullptr;
}
void CodeGen::visit(FuncDef &node) {
    auto *header = node.funcHeader();
    auto *funcSym = header ? header->symbol() : nullptr;
    auto *body = node.funcBody();
    if (!funcSym) {
        lastValue = nullptr;
        return;
    }
    // save current function
    auto prevFn = CodeGenCtx::curFunction;
    auto prevBlock = ctx.getBuilder().GetInsertBlock();
    auto prevEnv = currentEnv;

    const bool is_main = node.isEntrypoint();
    const bool is_method = node.isMethod() && ctx.curCls != nullptr;
    auto sig = ctx.buildSignature(funcSym, is_main, false);
    // get function type (params type + return type)
    auto funcType = llvm::FunctionType::get(sig.retTy, sig.paramTys, false);

    // Create entry basic block for function body
    auto newFunction = ctx.createFunction(funcSym, funcType, currentEnv);

    CodeGenCtx::curFunction = newFunction;

    unsigned idx = 0;
    // store parameters in function
    // function Environment
    currentEnv = std::make_shared<Environment>(currentEnv);
    if (is_method) {
        // we add this pointer to method
        auto argIt = newFunction->arg_begin();
        ctx.curThisCls = &(*argIt);
        string selfName = ctx.curCls->getName().data() + string("_self");
        ctx.curThisCls->setName(selfName);
        ++argIt;
        // handle other params
        for (; argIt != newFunction->arg_end(); ++argIt) {
            auto paramSym = funcSym->getParams().at(idx++);
            argIt->setName(paramSym->getName());

            auto argBinding =
                ctx.createLocalVariable(paramSym, argIt->getType(), currentEnv);
            ctx.getBuilder().CreateStore(&(*argIt), argBinding);
        }
    } else {
        ctx.curThisCls = nullptr;
        for (auto &arg: newFunction->args()) {
            auto paramSym = funcSym->getParams().at(idx++);
            arg.setName(paramSym->getName());

            auto argBinding =
                ctx.createLocalVariable(paramSym, arg.getType(), currentEnv);
            ctx.getBuilder().CreateStore(&arg, argBinding);
        }
    }

    body->accept(*this);

    llvm::BasicBlock *BB = ctx.getBuilder().GetInsertBlock();
    if (BB && !BB->getTerminator()) {
        auto *retTy = newFunction->getReturnType();

        if (retTy->isVoidTy()) {
            ctx.getBuilder().CreateRetVoid();
        } else if (retTy->isIntegerTy()) {
            ctx.getBuilder().CreateRet(llvm::ConstantInt::get(retTy, 0));
        } else {
            llvm::errs() << "FuncDef: unsupported return type for auto-return\n";
        }
    }

    ctx.getBuilder().SetInsertPoint(prevBlock);
    CodeGenCtx::curFunction = prevFn;
    currentEnv = prevEnv;

    // Validate the generated code, checking for consistency.
    verifyFunction(*newFunction);

    lastValue = nullptr;
}

void CodeGen::visit(ClassDef &node) {
    string clsName = node.identifier();
    auto parent = nullptr;
    const auto &fields = node.fieldList();
    const auto &methods = node.methodList();

    // create cls
    ctx.curCls = llvm::StructType::create(ctx.getLLVMContext(), clsName);
    ctx.getModule().getOrInsertGlobal(clsName, ctx.curCls);
    // TODO: inherence
    if (parent != nullptr) {
        // do something
    } else {
        auto clsInfo =
            std::make_unique<CodeGenCtx::ClassInfo>(ctx.curCls, parent);
        ctx.addClsMap(clsName, std::move(clsInfo));
    }
    // populate class with fields and methods
    ctx.buildClassInfo(ctx.curCls, node, currentEnv);
    // compile class body
    // we dont need to compile variables in class because it is already compiled in 'buildClass'
    for (auto &field: fields) {
        field->accept(*this);
    }
    for (auto &method: methods) {
        method->accept(*this);
    }
    ctx.curCls = nullptr;// after compiling, reset it
}
void CodeGen::visit(Block &node) {
    Environment::Env env = std::make_shared<Environment>(currentEnv);
    EnvironmentGuard env_guard{*this, env};
    for (auto &stmt: node.statementsList()) {
        stmt->accept(*this);
    }
}

void CodeGen::visit(SkipStmt &node) {}
void CodeGen::visit(ExitStmt &node) {}
void CodeGen::visit(AssignStmt &node) {
    llvm::Value *rhsValue = nullptr;// rval or lval
    llvm::Value *lhsAddr = nullptr; // lval address

    if (auto *rhs = node.right()) {
        rhs->accept(*this);
        rhsValue = lastValue;
    }
    if (auto *lhs = node.left()) {
        lhs->accept(*this);
        lhsAddr = lastValue;
    }
    if (lhsAddr && rhsValue) {
        ctx.getBuilder().CreateStore(rhsValue, lhsAddr);// store lhs -> rhs
    }
    lastValue = nullptr;
}
void CodeGen::visit(ReturnStmt &node) {
    node.returnValue()->accept(*this);
    llvm::Value *retValue = lastValue;
    if (CodeGenCtx::curFunction->getReturnType()->isVoidTy()) {
        ctx.getBuilder().CreateRetVoid();
    } else {
        ctx.getBuilder().CreateRet(retValue);
    }
}
void CodeGen::visit(ProcCall &node) {
    lastValue = nullptr;
    auto *calleeSym = node.funcSymbol();
    if (!calleeSym) {
        return;
    }
    makeCall(calleeSym, node.arguments());
}
void CodeGen::visit(BreakStmt &node) {}
void CodeGen::visit(ContinueStmt &node) {}
void CodeGen::visit(IfStmt &node) {
    llvm::BasicBlock *curBB = ctx.getBuilder().GetInsertBlock();
    llvm::Function *parentFunc = curBB->getParent();

    llvm::BasicBlock *endBlock = ctx.createBasicBlock("if.end", parentFunc);
    Block *elseBlockNode = node.elseBlock();
    llvm::BasicBlock *elseBlock =
        elseBlockNode ? ctx.createBasicBlock("if.else", parentFunc) : endBlock;

    // collect all branches
    vec<std::pair<Cond *, Block *>> branches;
    // add if-then branch
    branches.emplace_back(node.conditionExpr(), node.thenBlock());
    for (const auto &elifPair: node.elifs()) {
        branches.emplace_back(elifPair.first.get(), elifPair.second.get());
    }

    llvm::BasicBlock *condBB = curBB;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        Cond *condNode = branches[i].first;
        Block *bodyNode = branches[i].second;
        bool lastBranch = (i == branches.size() - 1);

        llvm::BasicBlock *thenBlock =
            ctx.createBasicBlock("if.then", parentFunc);
        llvm::BasicBlock *falseBlock =
            lastBranch ? elseBlock
                       : ctx.createBasicBlock("if.else", parentFunc);

        // generate condition
        ctx.getBuilder().SetInsertPoint(condBB);
        condNode->accept(*this);
        llvm::Value *condValue = lastValue;
        ctx.getBuilder().CreateCondBr(condValue, thenBlock, falseBlock);
        // generate then block
        ctx.getBuilder().SetInsertPoint(thenBlock);
        bodyNode->accept(*this);
        // if then block is not terminated, branch to end
        llvm::BasicBlock *thenExit = ctx.getBuilder().GetInsertBlock();
        if (thenExit && !thenExit->getTerminator()) {
            ctx.getBuilder().CreateBr(endBlock);
        }
        condBB = falseBlock;

        // generate else block
        if (elseBlockNode) {
            lastValue = nullptr;
            ctx.getBuilder().SetInsertPoint(elseBlock);
            elseBlockNode->accept(*this);
            llvm::BasicBlock *elseExit = ctx.getBuilder().GetInsertBlock();
            if (elseExit && !elseExit->getTerminator()) {
                ctx.getBuilder().CreateBr(endBlock);
            }
        }
    }
    // continue at end block
    ctx.getBuilder().SetInsertPoint(endBlock);
    lastValue = nullptr;
}
void CodeGen::visit(LoopStmt &node) {
    llvm::BasicBlock *curBlock = ctx.getBuilder().GetInsertBlock();
    auto *parentFunc = curBlock ? curBlock->getParent() : nullptr;
    if (!parentFunc) {
        lastValue = nullptr;
        return;
    }

    auto *condNode = node.conditionExpr();
    auto *bodyNode = node.loopBody();

    auto *condBlock = ctx.createBasicBlock("loop.cond", parentFunc);
    auto *bodyBlock = ctx.createBasicBlock("loop.body", parentFunc);
    auto *endBlock = ctx.createBasicBlock("loop.end", parentFunc);

    if (!curBlock->getTerminator()) {
        ctx.getBuilder().CreateBr(condBlock);
    }

    // compile while
    ctx.getBuilder().SetInsertPoint(condBlock);
    condNode->accept(*this);
    ctx.getBuilder().CreateCondBr(lastValue, bodyBlock, endBlock);
    // compile body
    ctx.getBuilder().SetInsertPoint(bodyBlock);
    bodyNode->accept(*this);
    if (!ctx.getBuilder().GetInsertBlock()->getTerminator()) {
        ctx.getBuilder().CreateBr(condBlock);
    }
    // end while
    ctx.getBuilder().SetInsertPoint(endBlock);
}

void CodeGen::visit(IdLVal &node) {
    auto sym = node.symbol();
    if (auto var = currentEnv->lookup(sym)) {
        lastValue = var;
        return;
    }
    // !it should be a class field
    if (ctx.curCls != nullptr) {
        string clsName = ctx.curCls->getName().data();
        CodeGenCtx::ClassInfo *clsInfo = ctx.lookupClsMap(clsName);
        auto &fields = clsInfo->fieldsMap;
        if (fields.find(sym->getName()) != fields.end()) {
            string ptrName = string("p") + sym->getName();
            size_t fieldIndex = ctx.getFieldIndex(ctx.curCls, sym->getName());
            lastValue = ctx.getBuilder().CreateStructGEP(ctx.curCls, ctx.curThisCls, fieldIndex, ptrName);
        }
    }
}
void CodeGen::visit(StringLiteralLVal &node) {
    const std::string &literal = node.literal();
    // remove the surrounding quotes
    string content = literal.substr(1, literal.size() - 2);
    auto *constStr =
        llvm::ConstantDataArray::getString(ctx.getLLVMContext(), content, true);
    auto *arrayTy = constStr->getType();

    static int strCounter = 0;
    std::string globalName = ".str." + std::to_string(strCounter++);

    auto *global = new llvm::GlobalVariable(ctx.getModule(), arrayTy, true, llvm::GlobalValue::PrivateLinkage, constStr, globalName);
    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    lastValue = global;
}
void CodeGen::visit(IndexLVal &node) {
    llvm::Value *basePtr = nullptr;
    if (auto *base = node.baseExpr()) {
        base->accept(*this);
        basePtr = lastValue;
    }
    if (!basePtr) {
        lastValue = nullptr;
        return;
    }
    llvm::Value *indexVal = nullptr;
    if (auto *idx = node.indexExpr()) {
        idx->accept(*this);
        indexVal = lastValue;
    }
    if (!indexVal) {
        lastValue = nullptr;
        return;
    }
    // basePtr -> address of the array in memory
    // indexVal -> evaluated expression inside brackets
    if (!basePtr->getType()->isIntegerTy(32)) {
        indexVal = ctx.getBuilder().CreateIntCast(
            indexVal, llvm::Type::getInt32Ty(ctx.getLLVMContext()), true,
            "idx.cast"
        );
    }

    auto *baseElemType = basePtr->getType();
    if (!baseElemType->isPointerTy()) {
        lastValue = nullptr;
        return;
    }
    const auto *baseSema =
        node.baseExpr() ? node.baseExpr()->type().get() : nullptr;
    const auto *elemSema = node.type() ? node.type().get() : nullptr;
    llvm::Type *elemType = elemSema ? ctx.getLLVMType(*elemSema) : nullptr;
    if (!elemType) {
        lastValue = nullptr;
        return;
    }
    if (baseSema && baseSema->getKind() == SemaType::TypeKind::ARRAY) {
        // for array type, we need to get the element type
        const auto &arrayTy = static_cast<const ArrayType &>(*baseSema);
        if (arrayTy.size()) {
            auto arrayLLVMTy = ctx.getLLVMType(*baseSema, false);
            auto zero = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(ctx.getLLVMContext()), 0
            );
            lastValue = ctx.getBuilder().CreateInBoundsGEP(
                arrayLLVMTy, basePtr, {zero, indexVal}, "array.idx"
            );
            return;
        }
        lastValue = ctx.getBuilder().CreateInBoundsGEP(elemType, basePtr, indexVal, "ptr.idx");
    }
    lastValue = ctx.getBuilder().CreateInBoundsGEP(elemType, basePtr, indexVal, "elem.idx");
}
void CodeGen::visit(MemberAccessLVal &node) {
}
void CodeGen::visit(IntConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(32, node.getValue(), true));
}
void CodeGen::visit(CharConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, node.getValue(), false));
}
void CodeGen::visit(TrueConst &node) {
    lastValue =
        llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, 1, false));
}
void CodeGen::visit(FalseConst &node) {
    lastValue =
        llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, 0, false));
}
void CodeGen::visit(LValueExpr &node) {
    node.lvalue()->accept(*this);
    llvm::Value *lvalAddr = lastValue;

    if (!lvalAddr) {
        lastValue = nullptr;
        return;
    }
    auto semType = node.type();
    // load value from lval address
    llvm::Type *loadType = ctx.getLLVMType(*semType);
    lastValue = ctx.getBuilder().CreateLoad(loadType, lvalAddr);
}
void CodeGen::visit(ParenExpr &node) {
    if (auto *inner = node.innerExpr()) {
        inner->accept(*this);
    }
}
void CodeGen::visit(FuncCall &node) {
    auto funcSym = node.funcSymbol();
    auto &args = node.arguments();
    llvm::Function *func = currentEnv->lookupFunc(funcSym);
    vec<llvm::Value *> llvmArgs;
    for (size_t i = 0; i < args.size(); ++i) {
        args[i]->accept(*this);
        auto paramType = func->getArg(i)->getType();
        auto bitCastArgVal = ctx.getBuilder().CreateBitCast(lastValue, paramType);
        llvmArgs.push_back(bitCastArgVal);
    }
    lastValue = ctx.getBuilder().CreateCall(func, llvmArgs);
}
void CodeGen::visit(MemberAccessExpr &node) {
    auto fieldSym = node.memberSymbol();
    string fieldName = fieldSym->getName();
    auto instExpr = node.object();

    static_cast<LValueExpr *>(instExpr)->lvalue()->accept(*this);
    llvm::Value *instance = lastValue;
    string clsName;
    // TODO: ast node do not need to store lval expression but to just store lval
    if (auto lvalExpr = dynamic_cast<LValueExpr *>(instExpr)) {
        auto semaTy = lvalExpr->lvalue()->type();
        if (auto instTy = std::dynamic_pointer_cast<const InstanceType>(semaTy)) {
            clsName = instTy->getClassName();
        }
    }
    auto cls = llvm::StructType::getTypeByName(ctx.getLLVMContext(), clsName);
    size_t fieldIndex = ctx.getFieldIndex(cls, fieldName);
    string ptrName = string("p") + fieldName;
    auto address = ctx.getBuilder().CreateStructGEP(cls, instance, fieldIndex, ptrName);
    lastValue = ctx.getBuilder().CreateLoad(cls->getElementType(fieldIndex), address, fieldName);
}
void CodeGen::visit(MethodCall &node) {
    auto callee = node.object();
    auto methodSym = node.methodSymbol();
    string methodName = methodSym->getName();
    // TODO: same problem as before
    // we do not use callee->accept because LValueExpr would do load Instruction
    static_cast<LValueExpr *>(callee)->lvalue()->accept(*this);

    llvm::Value *instance = lastValue;

    llvm::StructType *vTableType = nullptr;
    llvm::Value *vTable = nullptr;

    string clsName;
    if (auto lvalExpr = dynamic_cast<LValueExpr *>(callee)) {
        auto semaTy = lvalExpr->lvalue()->type();
        if (auto instTy = std::dynamic_pointer_cast<const InstanceType>(semaTy)) {
            clsName = instTy->getClassName();
        }
    }
    auto cls = llvm::StructType::getTypeByName(ctx.getLLVMContext(), clsName);

    // laod vtable
    string vTableName = clsName + "_vTable";
    vTableType = llvm::StructType::getTypeByName(ctx.getLLVMContext(), vTableName);
    auto vTableAddr = ctx.getBuilder().CreateStructGEP(cls, instance, VTABLE_INDEX, "vtable_addr");
    vTable = ctx.getBuilder().CreateLoad(llvm::PointerType::get(vTableType, 0), vTableAddr, "vtbale");

    // load method
    size_t methodIndex = ctx.getMethodIndex(cls, methodName);
    // auto methodType = (llvm::FunctionType *) vTableType->getElementType(methodIndex);
    auto methodPtrType = vTableType->getElementType(methodIndex);
    llvm::FunctionType *methodType = nullptr;

    if (auto ptrTy = llvm::dyn_cast<llvm::PointerType>(methodPtrType)) {
        auto clsInfo = ctx.lookupClsMap(clsName);
        auto methodFunc = clsInfo->methodsMap[methodName];
        methodType = methodFunc->getFunctionType();
    }

    auto methodAddr = ctx.getBuilder().CreateStructGEP(vTableType, vTable, methodIndex, "method_addr");

    auto methodPtr = ctx.getBuilder().CreateLoad(
        llvm::PointerType::get(ctx.getLLVMContext(), 0), methodAddr, "method_ptr"
    );

    std::vector<llvm::Value *> args;
    args.push_back(instance);// this pointer

    for (auto &arg: node.arguments()) {
        arg->accept(*this);
        args.push_back(lastValue);
    }

    lastValue = ctx.getBuilder().CreateCall(methodType, methodPtr, args, "method_call");
}
void CodeGen::visit(NewExpr &node) {
    string clsName = node.getCotorName();
    auto &args = node.getArgs();
    auto cls = llvm::StructType::getTypeByName(ctx.getLLVMContext(), clsName);
    auto instance = ctx.mallocInstance(cls, "obj");

    vec<llvm::Value *> ctorArgs{instance};
    for (auto &arg: args) {
        arg->accept(*this);
        ctorArgs.push_back(lastValue);
    }
    auto ctor = ctx.getModule().getFunction(clsName + "_constructor");
    ctx.getBuilder().CreateCall(ctor, ctorArgs);

    lastValue = instance;
}
void CodeGen::visit(UnaryExpr &node) {
    node.operandExpr()->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand)
        return;

    switch (node.opKind()) {
        case UnOp::Plus:
            lastValue = operand;
            break;
        case UnOp::Minus:
            lastValue = ctx.getBuilder().CreateNeg(operand, "neg");
            break;
        case UnOp::Not: {
            auto *zero = llvm::ConstantInt::get(operand->getType(), 0);
            auto *isZero = ctx.getBuilder().CreateICmpEQ(operand, zero, "not.cmp");
            // The result of ICmp is i1 (1-bit). We need to extend it back to i8
            // (byte).
            lastValue =
                ctx.getBuilder().CreateZExt(isZero, operand->getType(), "not.res");
        } break;
    }
}
void CodeGen::visit(BinaryExpr &node) {
    node.leftExpr()->accept(*this);
    llvm::Value *lhs = lastValue;

    node.rightExpr()->accept(*this);
    llvm::Value *rhs = lastValue;

    if (!lhs || !rhs) {
        lastValue = nullptr;
        return;
    }

    switch (node.opKind()) {
        case BinOp::Add:
            lastValue = ctx.getBuilder().CreateAdd(lhs, rhs, "add");
            break;
        case BinOp::Sub:
            lastValue = ctx.getBuilder().CreateSub(lhs, rhs, "sub");
            break;
        case BinOp::Mul:
            lastValue = ctx.getBuilder().CreateMul(lhs, rhs, "mul");
            break;
        case BinOp::Div:
            // Assuming signed division for 'int', unsigned for 'byte'
            if (lhs->getType()->isIntegerTy(8)) {
                lastValue = ctx.getBuilder().CreateUDiv(lhs, rhs, "div.u");
            } else {
                lastValue = ctx.getBuilder().CreateSDiv(lhs, rhs, "div.s");
            }
            break;
        case BinOp::Mod:
            if (lhs->getType()->isIntegerTy(8)) {
                lastValue = ctx.getBuilder().CreateURem(lhs, rhs, "rem.u");
            } else {
                lastValue = ctx.getBuilder().CreateSRem(lhs, rhs, "rem.s");
            }
            break;
        case BinOp::AndBits:
            lastValue = ctx.getBuilder().CreateAnd(lhs, rhs, "and.bits");
            break;
        case BinOp::OrBits:
            lastValue = ctx.getBuilder().CreateOr(lhs, rhs, "or.bits");
            break;
        case BinOp::Eq:
            lastValue = ctx.getBuilder().CreateICmpEQ(lhs, rhs, "eq");
            break;
        case BinOp::Ne:
            lastValue = ctx.getBuilder().CreateICmpNE(lhs, rhs, "neq");
            break;
        case BinOp::Lt:
            lastValue = ctx.getBuilder().CreateICmpSLT(lhs, rhs, "lt");
            break;
        case BinOp::Le:
            lastValue = ctx.getBuilder().CreateICmpSLE(lhs, rhs, "le");
            break;
        case BinOp::Gt:
            lastValue = ctx.getBuilder().CreateICmpSGT(lhs, rhs, "gt");
            break;
        case BinOp::Ge:
            lastValue = ctx.getBuilder().CreateICmpSGE(lhs, rhs, "ge");
            break;
        case BinOp::And:
            lastValue = ctx.getBuilder().CreateAnd(lhs, rhs, "and.logical");
            break;
        case BinOp::Or:
            lastValue = ctx.getBuilder().CreateOr(lhs, rhs, "or.logical");
            break;
    }
}

void CodeGen::visit(ArrayExpr &node) {
    const auto &elems = node.getElements();
    if (elems.empty()) {
        return;
    }

    const auto semaTy = node.type();
    const auto *arraySema =
        semaTy ? dynamic_cast<const ArrayType *>(semaTy.get()) : nullptr;
    if (!arraySema || !arraySema->size().has_value()) {
        return;// only arrat literal with length supported
    }

    llvm::Type *elemTy = ctx.getLLVMType(*arraySema->elementType());
    if (!elemTy) {
        return;
    }

    auto *arrayTy = llvm::ArrayType::get(elemTy, *arraySema->size());
    auto *arrayPtr = ctx.getBuilder().CreateAlloca(arrayTy, nullptr, "arr.lit");
    auto *zero =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.getLLVMContext()), 0);

    for (std::size_t i = 0; i < elems.size(); ++i) {
        elems[i]->accept(*this);
        llvm::Value *elemVal = lastValue;
        if (!elemVal) {
            continue;
        }
        // 简单的整数/指针类型对齐
        if (elemVal->getType() != elemTy) {
            if (elemVal->getType()->isIntegerTy() && elemTy->isIntegerTy()) {
                elemVal = ctx.getBuilder().CreateIntCast(elemVal, elemTy, true, "arr.elem.cast");
            } else if (elemVal->getType()->isPointerTy() &&
                       elemTy->isPointerTy()) {
                elemVal = ctx.getBuilder().CreateBitCast(elemVal, elemTy, "arr.elem.cast");
            }
        }
        auto *idxConst = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(ctx.getLLVMContext()), i
        );
        auto *slot = ctx.getBuilder().CreateInBoundsGEP(
            arrayTy, arrayPtr, {zero, idxConst}, "arr.slot"
        );
        ctx.getBuilder().CreateStore(elemVal, slot);
    }

    lastValue = ctx.getBuilder().CreateLoad(arrayTy, arrayPtr, "arr.val");
}

void CodeGen::visit(ExprCond &node) { node.expression()->accept(*this); }

// make sure that function symbol has corresponding LLVM function in module
llvm::Function *
CodeGen::ensureLLVMFunction(FuncSymbol *funcSym, const CodeGenCtx::FuncSignature &sig, const bool is_main) {
    // look up existing function symbol in module
    if (!funcSym)
        return nullptr;
    auto *llvmFunc = currentEnv->lookupFunc(funcSym);
    if (llvmFunc)
        return llvmFunc;

    std::vector<llvm::Type *> fnArgTypes;
    fnArgTypes.insert(fnArgTypes.end(), sig.paramTys.begin(), sig.paramTys.end());

    auto *llvmFnTy = llvm::FunctionType::get(sig.retTy, fnArgTypes, false);
    llvmFunc = llvm::Function::Create(
        llvmFnTy, llvm::GlobalValue::ExternalLinkage,
        is_main ? "main" : funcSym->getName(), &ctx.getModule()
    );
    currentEnv->bindFunc(funcSym, llvmFunc);
    return llvmFunc;
}

llvm::Value *CodeGen::makeCall(FuncSymbol *calleeSym, const vec<uptr<Expr>> &args) {
    if (!calleeSym)
        return nullptr;

    // declared function and we can find its LLVM function, parameters and
    // return type
    llvm::Function *callee = currentEnv->lookupFunc(calleeSym);
    if (!callee)

        return nullptr;

    auto *functionTy = callee->getFunctionType();
    vec<llvm::Value *> callArgs;
    callArgs.reserve(functionTy->getNumParams());
    // bool needsStaticLink = calleeSym->definingFunc() != nullptr;
    unsigned paramIdx = 0;

    const auto &params = calleeSym->getParams();
    const unsigned totalParams = params.size();
    // generally, they should be equal
    // in case we have optional parameters
    const unsigned realCount = std::min<unsigned>(totalParams, args.size());

    // helper to get lvalue node
    auto getLValueNode = [](Expr *expr) -> Lval * {
        while (expr) {
            if (auto *lvalExpr = dynamic_cast<LValueExpr *>(expr)) {
                return lvalExpr->lvalue();
            }
            if (auto *parenExpr = dynamic_cast<ParenExpr *>(expr)) {
                expr = parenExpr->innerExpr();
                continue;
            }
            break;
        }
        return nullptr;
    };

    // args 是实参，我们会比较实参和形参，进行cast
    for (std::size_t i = 0; i < realCount; ++i) {
        auto *expr = args[i].get();
        auto *paramSym = params[i];

        llvm::Type *parmTy = functionTy->getFunctionParamType(paramIdx++);
        llvm::Value *argValue = nullptr;

        if (expr) {
            const bool byRef =
                paramSym && paramSym->getPass() == Symbol::ParamPass::BY_REF;
            if (byRef) {
                if (auto *lvalNode = getLValueNode(expr)) {
                    lvalNode->accept(*this);
                    argValue = lastValue;
                }
                if (!argValue) {
                    expr->accept(*this);
                    argValue = lastValue;
                }
            } else {
                expr->accept(*this);
                argValue = lastValue;
            }
        }
        if (!argValue) {
            argValue = llvm::Constant::getNullValue(parmTy);
        }
        lastValue = nullptr;

        if (argValue->getType() != parmTy) {
            if (argValue->getType()->isPointerTy() && parmTy->isPointerTy()) {
                argValue = ctx.getBuilder().CreateBitCast(argValue, parmTy, "arg.cast");

            } else if (argValue->getType()->isIntegerTy() &&
                       parmTy->isIntegerTy()) {
                argValue = ctx.getBuilder().CreateIntCast(argValue, parmTy, true, "arg.cast");
            }
        }
        callArgs.push_back(argValue);
    }
    // TODO: handle optional parameters
    return ctx.getBuilder().CreateCall(callee, callArgs);
}

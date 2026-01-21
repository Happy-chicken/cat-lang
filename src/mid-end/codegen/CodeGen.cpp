#include "CodeGen.hpp"
#include "AST.hpp"
#include "CodeGenCtx.hpp"
#include "Environment.hpp"
#include "Symbol.hpp"
#include <cstddef>
#include <llvm-20/llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <memory>

llvm::Value *CodeGen::lastValue = nullptr;

// ------------------------------------------------------------------------------------------------------
// some helper function for Structures and Functions
// ------------------------------------------------------------------------------------------------------

// factory function to build function signature from FuncSymbol
static FuncSignature buildSignature(CodeGenCtx &ctx, const FuncSymbol *funcSym, bool isMain = false) {
    FuncSignature sig;
    sig.paramTys.clear();

    // build parameter types of function
    if (funcSym) {
        for (const auto &p: funcSym->getParams()) {
            const bool byRef = p->getPass() == Symbol::ParamPass::BY_REF;
            llvm::Type *ty = byRef ? llvm::PointerType::get(ctx.getLLVMContext(), 0)
                                   : ctx.getLLVMType(*p->getType(), true);
            sig.paramTys.push_back(ty);
        }
    }

    if (!isMain) {
        auto *fnSig = funcSym ? static_cast<const FuncType *>(funcSym->getType().get()) : nullptr;
        sig.retTy = (fnSig && fnSig->returnType()) ? ctx.getLLVMType(*fnSig->returnType())
                                                   : llvm::Type::getVoidTy(ctx.getLLVMContext());
    } else {
        // if this functino is not main funciton, we just return 0;
        sig.retTy = llvm::Type::getInt32Ty(ctx.getLLVMContext());
        sig.paramTys.clear();
    }

    return sig;
}

static bool hasNestedFuncDefs(const vec<uptr<Def>> &defs) {
    for (const auto &defn: defs) {
        if (dynamic_cast<FuncDef *>(defn.get())) {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------------------------
CodeGen::CodeGen(CodeGenCtx &ctx) : ctx(ctx) {
    setupGlobalEnvironment();
}

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
    auto syms = node.symbols();
    auto type = node.declaredType();
    const auto &initExprOpt = node.initExpr();
    // if there is no initializer
    if (!initExprOpt.has_value()) {
        for (auto *sym: syms) {
            string name = sym->getName();
            llvm::Type *llvmType = ctx.getLLVMType(*sym->getType());
            auto varAddr = ctx.createLocalVariable(sym, llvmType, currentEnv);
            lastValue = varAddr;
        }
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

    auto sig = buildSignature(ctx, funcSym);
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
    auto sig = buildSignature(ctx, funcSym, is_main);
    // get function type (params type + return type)
    auto funcType = llvm::FunctionType::get(sig.retTy, sig.paramTys, false);

    // Create entry basic block for function body
    auto newFunction = ctx.createFunction(funcSym, funcType, currentEnv);

    CodeGenCtx::curFunction = newFunction;

    // store parameters in function
    currentEnv = std::make_shared<Environment>(currentEnv);
    unsigned idx = 0;
    for (auto &arg: newFunction->args()) {
        auto paramSym = funcSym->getParams().at(idx++);
        arg.setName(paramSym->getName());

        auto argBinding = ctx.createLocalVariable(paramSym, arg.getType(), currentEnv);
        ctx.getBuilder().CreateStore(&arg, argBinding);
    }


    body->accept(*this);

    llvm::BasicBlock *BB = ctx.getBuilder().GetInsertBlock();
    if (BB && !BB->getTerminator()) {
        if (is_main) {
            ctx.getBuilder().CreateRet(ctx.getBuilder().getInt32(0));
        }
    }

    ctx.getBuilder().SetInsertPoint(prevBlock);
    CodeGenCtx::curFunction = prevFn;
    currentEnv = prevEnv;

    // Validate the generated code, checking for consistency.
    verifyFunction(*newFunction);

    lastValue = nullptr;
}
void CodeGen::visit(ClassDef &node) {}
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
        ctx.getBuilder().CreateStore(rhsValue, lhsAddr);// store lhs, rhs
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
void CodeGen::visit(IfStmt &node) {}
void CodeGen::visit(LoopStmt &node) {}

void CodeGen::visit(IdLVal &node) {
    auto sym = node.symbol();
    lastValue = currentEnv->lookup(sym);
}
void CodeGen::visit(StringLiteralLVal &node) {
    const std::string &literal = node.literal();
    // remove the surrounding quotes
    string content = literal.substr(1, literal.size() - 2);
    auto *constStr = llvm::ConstantDataArray::getString(
        ctx.getLLVMContext(), content, true
    );
    auto *arrayTy = constStr->getType();

    static int strCounter = 0;
    std::string globalName = ".str." + std::to_string(strCounter++);

    auto *global = new llvm::GlobalVariable(
        ctx.getModule(),
        arrayTy,
        true,
        llvm::GlobalValue::PrivateLinkage,
        constStr,
        globalName
    );
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
        indexVal = ctx.getBuilder().CreateIntCast(indexVal, llvm::Type::getInt32Ty(ctx.getLLVMContext()), true, "idx.cast");
    }

    auto *baseElemType = basePtr->getType();
    if (!baseElemType->isPointerTy()) {
        lastValue = nullptr;
        return;
    }
    const auto *baseSema = node.baseExpr() ? node.baseExpr()->type().get() : nullptr;
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
            auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.getLLVMContext()), 0);
            lastValue = ctx.getBuilder().CreateInBoundsGEP(
                arrayLLVMTy,
                basePtr,
                {zero, indexVal},
                "array.idx"
            );
            return;
        }
        lastValue = ctx.getBuilder().CreateInBoundsGEP(
            elemType,
            basePtr,
            indexVal,
            "ptr.idx"
        );
    }
    lastValue = ctx.getBuilder().CreateInBoundsGEP(
        elemType,
        basePtr,
        indexVal,
        "elem.idx"
    );
}
void CodeGen::visit(MemberAccessLVal &node) {}
void CodeGen::visit(IntConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(32, node.getValue(), true));
}
void CodeGen::visit(CharConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, node.getValue(), false));
}
void CodeGen::visit(TrueConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, 1, false));
}
void CodeGen::visit(FalseConst &node) {
    lastValue = llvm::ConstantInt::get(ctx.getLLVMContext(), llvm::APInt(8, 0, false));
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
void CodeGen::visit(FuncCall &node) {}
void CodeGen::visit(MemberAccessExpr &node) {}
void CodeGen::visit(MethodCall &node) {}
void CodeGen::visit(UnaryExpr &node) {
    node.operandExpr()->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand) return;

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
            // The result of ICmp is i1 (1-bit). We need to extend it back to i8 (byte).
            lastValue = ctx.getBuilder().CreateZExt(isZero, operand->getType(), "not.res");
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
void CodeGen::visit(ExprCond &node) {}


// make sure that function symbol has corresponding LLVM function in module
llvm::Function *CodeGen::ensureLLVMFunction(FuncSymbol *funcSym, const FuncSignature &sig, const bool is_main) {
    // look up existing function symbol in module
    if (!funcSym) return nullptr;
    auto *llvmFunc = currentEnv->lookupFunc(funcSym);
    if (llvmFunc) return llvmFunc;

    std::vector<llvm::Type *> fnArgTypes;
    fnArgTypes.insert(fnArgTypes.end(), sig.paramTys.begin(), sig.paramTys.end());

    auto *llvmFnTy = llvm::FunctionType::get(sig.retTy, fnArgTypes, false);
    llvmFunc = llvm::Function::Create(llvmFnTy, llvm::GlobalValue::ExternalLinkage, is_main ? "main" : funcSym->getName(), &ctx.getModule());
    currentEnv->bindFunc(funcSym, llvmFunc);
    return llvmFunc;
}

llvm::Value *CodeGen::makeCall(FuncSymbol *calleeSym, const vec<uptr<Expr>> &args) {
    if (!calleeSym) return nullptr;

    // declared function and we can find its LLVM function, parameters and return type
    llvm::Function *callee = currentEnv->lookupFunc(calleeSym);
    if (!callee) return nullptr;

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
            const bool byRef = paramSym && paramSym->getPass() == Symbol::ParamPass::BY_REF;
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

            } else if (argValue->getType()->isIntegerTy() && parmTy->isIntegerTy()) {
                argValue = ctx.getBuilder().CreateIntCast(argValue, parmTy, true, "arg.cast");
            }
        }
        callArgs.push_back(argValue);
    }
    // TODO: handle optional parameters
    return ctx.getBuilder().CreateCall(callee, callArgs);
}
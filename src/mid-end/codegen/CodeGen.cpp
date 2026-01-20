#include "CodeGen.hpp"
#include "CodeGenCtx.hpp"
#include "Environment.hpp"
#include "Symbol.hpp"
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

        auto argBinding = ctx.allocVar(paramSym, arg.getType(), currentEnv);
        ctx.getBuilder().CreateStore(&arg, argBinding);
    }

    body->accept(*this);

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
void CodeGen::visit(ProcCall &node) {}
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
    auto *constStr = llvm::ConstantDataArray::getString(
        ctx.getLLVMContext(), literal, true
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
void CodeGen::visit(IndexLVal &node) {}
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
void CodeGen::visit(LValueExpr &node) {}
void CodeGen::visit(ParenExpr &node) {}
void CodeGen::visit(FuncCall &node) {}
void CodeGen::visit(MemberAccessExpr &node) {}
void CodeGen::visit(MethodCall &node) {}
void CodeGen::visit(UnaryExpr &node) {}
void CodeGen::visit(BinaryExpr &node) {}
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

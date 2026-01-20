#pragma once
#include "AST.hpp"
#include "ASTVisitor.hpp"
#include "CodeGenCtx.hpp"
#include "Diagnostics.hpp"
#include "Environment.hpp"
#include "Location.hpp"
#include "Symbol.hpp"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>


class CodeGen : public AstVisitor {
public:
    explicit CodeGen(CodeGenCtx &codegenctx);
    virtual ~CodeGen() = default;
    void compile(sptr<Program> root) {
        root->accept(*this);
        std::error_code errorCode;
        llvm::raw_fd_ostream outLL("./out.ll", errorCode);
        ctx.getModule().print(outLL, nullptr);
    }

    CodeGenCtx &getContext() {
        return ctx;
    }

    void visit(Type &node);
    void visit(FuncParameterType &node);
    void visit(Program &node);
    void visit(FuncParameterDef &node);
    void visit(Header &node);
    void visit(VarDef &node);
    void visit(FuncDecl &node);
    void visit(FuncDef &node);
    void visit(ClassDef &node);
    void visit(Block &node);

    void visit(SkipStmt &node);
    void visit(ExitStmt &node);
    void visit(AssignStmt &node);
    void visit(ReturnStmt &node);
    void visit(ProcCall &node);
    void visit(BreakStmt &node);
    void visit(ContinueStmt &node);
    void visit(IfStmt &node);
    void visit(LoopStmt &node);

    void visit(IdLVal &node);
    void visit(StringLiteralLVal &node);
    void visit(IndexLVal &node);
    void visit(MemberAccessLVal &node);
    void visit(IntConst &node);
    void visit(CharConst &node);
    void visit(TrueConst &node);
    void visit(FalseConst &node);
    void visit(LValueExpr &node);
    void visit(ParenExpr &node);
    void visit(FuncCall &node);
    void visit(MemberAccessExpr &node);
    void visit(MethodCall &node);
    void visit(UnaryExpr &node);
    void visit(BinaryExpr &node);
    void visit(ExprCond &node);

    llvm::Function *ensureLLVMFunction(FuncSymbol *funcSym, const FuncSignature &sig, const bool is_main = false);

public:
    void setupGlobalEnvironment() {
        Environment::ValueMap globalObjects{
            {new VarSymbol("VERSION", nullptr, Location(0, 0)), ctx.getBuilder().getInt32(42)},
        };
        Environment::ValueMap globalRecords{};

        for (auto &entry: globalObjects) {
            globalRecords[entry.first] = ctx.createGlobalVariable(entry.first->getName(), (llvm::Constant *) entry.second);
        }
        globalEnv = std::make_shared<Environment>(nullptr, globalRecords, Environment::FuncMap{});
    }

    class EnvironmentGuard {
    public:
        EnvironmentGuard(CodeGen &ir_generator, Environment::Env enclosing_env) : ir_generator{ir_generator}, previous_env{ir_generator.currentEnv} {
            ir_generator.currentEnv = std::move(enclosing_env);
        }

        ~EnvironmentGuard() {
            ir_generator.currentEnv = std::move(previous_env);
        }

    private:
        CodeGen &ir_generator;
        std::shared_ptr<Environment> previous_env;
    };


private:
    CodeGenCtx &ctx;
    // Result:
    // For Expression nodes: the most recently evaluated computed data result
    // For L-value nodes: memory address
    // For Statements/voids: nullptr
    static llvm::Value *lastValue;
    Environment::Env globalEnv = nullptr;    // global environment
    Environment::Env &currentEnv = globalEnv;// current environment
    llvm::Value *makeCall(const FuncSymbol *calleeSym, const vec<uptr<Expr>> &args);
};

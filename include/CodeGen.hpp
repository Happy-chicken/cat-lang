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
#include <llvm/IR/Type.h>
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

    CodeGenCtx &getContext() { return ctx; }

    Environment::Env getGlobalEnvironment() const { return globalEnv; }

    void visit(Type &node) override;
    void visit(FuncParameterType &node) override;
    void visit(Program &node) override;
    void visit(FuncParameterDef &node) override;
    void visit(Header &node) override;
    void visit(VarDef &node) override;
    void visit(FuncDecl &node) override;
    void visit(FuncDef &node) override;
    void visit(ClassDef &node) override;
    void visit(Block &node) override;

    void visit(SkipStmt &node) override;
    void visit(ExitStmt &node) override;
    void visit(AssignStmt &node) override;
    void visit(ReturnStmt &node) override;
    void visit(ProcCall &node) override;
    void visit(BreakStmt &node) override;
    void visit(ContinueStmt &node) override;
    void visit(IfStmt &node) override;
    void visit(LoopStmt &node) override;

    void visit(IdLVal &node) override;
    void visit(StringLiteralLVal &node) override;
    void visit(IndexLVal &node) override;
    void visit(MemberAccessLVal &node) override;
    void visit(IntConst &node) override;
    void visit(CharConst &node) override;
    void visit(TrueConst &node) override;
    void visit(FalseConst &node) override;
    void visit(LValueExpr &node) override;
    void visit(ParenExpr &node) override;
    void visit(FuncCall &node) override;
    void visit(MemberAccessExpr &node) override;
    void visit(MethodCall &node) override;
    void visit(NewExpr &node) override;
    void visit(UnaryExpr &node) override;
    void visit(BinaryExpr &node) override;
    void visit(ExprCond &node) override;
    void visit(ArrayExpr &node) override;

    llvm::Function *ensureLLVMFunction(FuncSymbol *funcSym, const CodeGenCtx::FuncSignature &sig, const bool is_main = false);

public:
    void setupGlobalEnvironment() {
        Environment::ValueMap globalObjects{
            {new VarSymbol("VERSION", nullptr, Location(0, 0)),
             ctx.getBuilder().getInt32(42)},
        };
        Environment::ValueMap globalRecords{};

        for (auto &entry: globalObjects) {
            globalRecords[entry.first] = ctx.createGlobalVariable(
                entry.first->getName(), (llvm::Constant *) entry.second
            );
        }
        globalEnv = std::make_shared<Environment>(nullptr, globalRecords, Environment::FuncMap{});
    }

    class EnvironmentGuard {
    public:
        EnvironmentGuard(CodeGen &ir_generator, Environment::Env enclosing_env)
            : ir_generator{ir_generator},
              previous_env{ir_generator.currentEnv} {
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
    llvm::Value *makeCall(FuncSymbol *calleeSym, const vec<uptr<Expr>> &args);
};

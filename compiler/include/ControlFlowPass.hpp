#pragma once
#include "AST.hpp"
#include "ASTVisitor.hpp"
#include "SemanticCtx.hpp"
#include "Symbol.hpp"

// second pass
// handle control flow: return statements, break/continue, if/else, loops
// reachablility analysis
class ControlFlowPass : public AstVisitor {
public:
    explicit ControlFlowPass(SemanticCtx &ctx) : semanticCtx(ctx) {};

    bool blockCanFallThrough(const Block *block);
    bool stmtCallFallThrough(const Stmt *stmt);

private:
    SemanticCtx &semanticCtx;

    struct FunctionInfo {
        string name;
        bool isProcedure;
        bool isEntryPoint;
    };

    vec<FunctionInfo> functionStack;
    vec<optional<string>> loopStack;


public:
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
    void visit(ArrayExpr &node) override;
    void visit(ExprCond &node) override;
};

#pragma once
#include "AST.hpp"
#include "ASTVisitor.hpp"
#include "Location.hpp"
#include "SemanticCtx.hpp"
class SemanticCtx;

struct ParamInfo {
    std::string name;
    Location loc;
    SemaTypePtr type;
    bool isByRef;
};

class SemanticPass : public AstVisitor {
public:
    SemanticPass(SemanticCtx &ctx) : semanticCtx(ctx) {}

    void visit(Type &node) override;
    void visit(FuncParameterType &node) override;

    void visit(Program &node) override;
    void visit(Header &node) override;
    void visit(FuncDef &node) override;
    void visit(VarDef &node) override;
    void visit(FuncParameterDef &node) override;
    void visit(Block &node) override;

    void visit(SkipStmt &node) override;
    void visit(ExitStmt &node) override;
    void visit(IfStmt &node) override;
    void visit(LoopStmt &node) override;
    void visit(ReturnStmt &node) override;
    void visit(AssignStmt &node) override;
    void visit(BreakStmt &node) override;
    void visit(ContinueStmt &node) override;
    void visit(ProcCall &node) override;

    void visit(BinaryExpr &node) override;
    void visit(UnaryExpr &node) override;
    void visit(LValueExpr &node) override;
    void visit(IdLVal &node) override;
    void visit(IndexLVal &node) override;
    void visit(ParenExpr &node) override;
    void visit(FuncCall &node) override;

    void visit(IntConst &node) override;
    void visit(CharConst &node) override;
    void visit(TrueConst &node) override;
    void visit(FalseConst &node) override;

    void visit(ExprCond &node) override;

private:
    SemanticCtx &semanticCtx;
};
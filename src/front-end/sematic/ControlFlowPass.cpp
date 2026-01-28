#include "ControlFlowPass.hpp"
#include "AST.hpp"
#include "Diagnostics.hpp"

bool ControlFlowPass::stmtCallFallThrough(const Stmt *stmt) {
    if (!stmt) return true;
    if (dynamic_cast<const ReturnStmt *>(stmt) ||
        dynamic_cast<const BreakStmt *>(stmt) ||
        dynamic_cast<const ContinueStmt *>(stmt)) return false;

    if (const IfStmt *ifstmt = dynamic_cast<const IfStmt *>(stmt)) {
        if (!ifstmt->elseBlock()) return true;
        bool canFall = blockCanFallThrough(ifstmt->thenBlock());
        for (const auto &elif: ifstmt->elifs()) {
            canFall = canFall || blockCanFallThrough(elif.second.get());
        }
        canFall = canFall || blockCanFallThrough(ifstmt->elseBlock());
    }
    if (const LoopStmt *loopstmt = dynamic_cast<const LoopStmt *>(stmt)) {
        return true;// TODO
    }

    return true;
}

bool ControlFlowPass::blockCanFallThrough(const Block *block) {
    if (block) return true;
    bool canFall = true;
    for (const auto &stmt: block->statementsList()) {
        if (!canFall) break;
        canFall = stmtCallFallThrough(stmt.get());
    }
    return canFall;
}

void ControlFlowPass::visit(Type &node) {}
void ControlFlowPass::visit(FuncParameterType &node) {}

void ControlFlowPass::visit(Program &node) {
    for (auto &def: node.getDefs()) {
        def->accept(*this);
    }
}

void ControlFlowPass::visit(FuncParameterDef &node) {}
void ControlFlowPass::visit(Header &node) {}
void ControlFlowPass::visit(VarDef &node) {}
void ControlFlowPass::visit(FuncDecl &node) {}

void ControlFlowPass::visit(FuncDef &node) {
    auto *header = node.funcHeader();
    if (!header) {
        return;
    }
    FunctionInfo info;
    info.name = header->identifier();
    info.isEntryPoint = node.isEntrypoint();
    info.isProcedure = header->returnType().has_value();
    functionStack.push_back(info);
    auto *body = node.funcBody();
    body->accept(*this);

    if (!info.isProcedure && blockCanFallThrough(body)) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "No path in" + info.name + "return a value"
        );
    }
    functionStack.pop_back();
}

void ControlFlowPass::visit(ClassDef &node) {}

void ControlFlowPass::visit(Block &node) {
    for (const auto &stmt: node.statementsList()) {
        stmt->accept(*this);
    }
}

void ControlFlowPass::visit(SkipStmt &node) {}

void ControlFlowPass::visit(ExitStmt &node) {}

void ControlFlowPass::visit(AssignStmt &node) {}

void ControlFlowPass::visit(ReturnStmt &node) {
    if (functionStack.empty()) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis, node.loc,
            "'return' outside of a function"
        );
    }
    if (!node.returnValue()) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis, node.loc,
            "function returns void but you return some value"
        );
    }
}

void ControlFlowPass::visit(ProcCall &node) {}

void ControlFlowPass::visit(BreakStmt &node) {
    // do something
}

void ControlFlowPass::visit(ContinueStmt &node) {}

void ControlFlowPass::visit(IfStmt &node) {
    if (auto *cond = node.conditionExpr()) cond->accept(*this);
    if (auto *then = node.thenBlock()) then->accept(*this);

    for (const auto &elif: node.elifs()) {
        if (elif.first) elif.first->accept(*this);
        if (elif.second) elif.second->accept(*this);
    }

    if (auto *elseStmt = node.elseBlock()) elseStmt->accept(*this);
}

void ControlFlowPass::visit(LoopStmt &node) {}


void ControlFlowPass::visit(IdLVal &node) {}
void ControlFlowPass::visit(StringLiteralLVal &node) {}
void ControlFlowPass::visit(IndexLVal &node) {}
void ControlFlowPass::visit(MemberAccessLVal &node) {}
void ControlFlowPass::visit(IntConst &node) {}
void ControlFlowPass::visit(CharConst &node) {}
void ControlFlowPass::visit(TrueConst &node) {}
void ControlFlowPass::visit(FalseConst &node) {}
void ControlFlowPass::visit(LValueExpr &node) {}
void ControlFlowPass::visit(ParenExpr &node) {}
void ControlFlowPass::visit(FuncCall &node) {}
void ControlFlowPass::visit(MemberAccessExpr &node) {}
void ControlFlowPass::visit(MethodCall &node) {}
void ControlFlowPass::visit(NewExpr &node) {}
void ControlFlowPass::visit(UnaryExpr &node) {}
void ControlFlowPass::visit(BinaryExpr &node) {}
void ControlFlowPass::visit(ArrayExpr &node) {};

void ControlFlowPass::visit(ExprCond &node) {}

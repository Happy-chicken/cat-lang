#pragma once

#include "AST.hpp"
#include "SemanticCtx.hpp"
#include "SemanticPass.hpp"
class PassDriver {
public:
    PassDriver(Program &ast)
        : astRoot(ast) {}

    void runSemanticPass(SemanticCtx &ctx);
    void runControlFlowPass(SemanticCtx &ctx);

private:
    Program &astRoot;
};
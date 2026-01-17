#include "PassDriver.hpp"

void PassDriver::runSemanticPass(SemanticCtx &ctx) {
    SemanticPass semanticPass(ctx);
    astRoot.accept(semanticPass);
}
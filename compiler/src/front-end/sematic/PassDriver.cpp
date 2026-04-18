#include "PassDriver.hpp"
#include "ControlFlowPass.hpp"
#include "SemanticCtx.hpp"

void PassDriver::runSemanticPass(SemanticCtx &ctx) {
  SemanticPass semanticPass(ctx);
  astRoot.accept(semanticPass);
}

void PassDriver::runControlFlowPass(SemanticCtx &ctx) {
  ControlFlowPass cfPass(ctx);
  astRoot.accept(cfPass);
}

#pragma once
#include "CodeGen.hpp"
#include "SemanticCtx.hpp"

namespace Catime {
    struct CatBuiltin {
        const char *runtimeName;
        const char *catName;
    };

    inline constexpr CatBuiltin builtinTable[] = {
        {"cat_print", "print"},

    };
    // class CodeGenCtx;
    // class SemanticCtx;

    void declareBuiltins(SemanticCtx &semCtx);
    void genBuiltins(SemanticCtx &semCtx, CodeGen &codegen);
}// namespace Catime

#include "SemanticCtx.hpp"
#include "Diagnostics.hpp"
#include "Location.hpp"

InsertResult SemanticCtx::declareSymbol(uptr<Symbol> sym, bool isRedeclared) {
    if (!sym) {
        return InsertResult::error();
    }
    auto result = symbol_table.declare(std::move(sym));
    const string name = sym->getName();
    const Location loc = sym->getLocation();

    if (isRedeclared && result.isRedeclared()) {
        diagnostics.report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, loc, "symbol '" + name + "' already declared in this scope.");
        if (result.symbol) {
            diagnostics.report(Diagnostics::Severity::Info, Diagnostics::Phase::SemanticAnalysis, result.symbol->getLocation(), "previous declaration is here.");
        }
    }
    return result;
}

LookupResult SemanticCtx::lookup(const string &name) const {
    return symbol_table.lookup(name);
}

LookupResult SemanticCtx::lookupLocalSymbol(const string &name) const {
    return symbol_table.currentScope().lookupLocal(name);
}

void SemanticCtx::enterFunction(sptr<FunctionFrame> frame) {
    function_stack.push_back(frame);
}

void SemanticCtx::leaveFunction() {
    if (!function_stack.empty()) {
        function_stack.pop_back();
    }
}

sptr<SemanticCtx::FunctionFrame> SemanticCtx::currentFunction() {
    if (function_stack.empty()) {
        return nullptr;
    }
    return function_stack.back();
}

const sptr<SemanticCtx::FunctionFrame> SemanticCtx::currentFunction() const {
    if (function_stack.empty()) {
        return nullptr;
    }
    return function_stack.back();
}
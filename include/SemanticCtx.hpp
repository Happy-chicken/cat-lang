#pragma once
#include "AST.hpp"
#include "Diagnostics.hpp"
#include "SemaType.hpp"
#include "Symbol.hpp"
#include "SymbolTable.hpp"
#include <cstddef>
class SemanticCtx {
public:
    SemanticCtx(SymbolTable &sym_tab, Diagnostics &diag) : symbol_table(sym_tab), diagnostics(diag) {}

    SymbolTable &getSymbolTable() { return symbol_table; }
    const SymbolTable &getSymbolTable() const { return symbol_table; }
    Diagnostics &getDiagnostics() { return diagnostics; }
    const Diagnostics &getDiagnostics() const { return diagnostics; }

    void beginScope() { symbol_table.beginScope(); }
    void endScope() { symbol_table.endScope(); }
    std::size_t scopeDepth() const { return symbol_table.scopeDepth(); }
    bool hasErrors() const { return diagnostics.hasErrors(); }

    InsertResult declareSymbol(uptr<Symbol> sym, bool isRedeclared = true);
    LookupResult lookup(const string &name) const;
    LookupResult lookupLocalSymbol(const string &name) const;

    struct FunctionFrame {
        FuncSymbol *symbol = nullptr;
        SemaTypePtr return_type;
        bool is_procedure = false;
    };

    void enterFunction(sptr<FunctionFrame> frame);
    void leaveFunction();
    sptr<FunctionFrame> currentFunction();
    const sptr<FunctionFrame> currentFunction() const;

private:
    SymbolTable &symbol_table;
    Diagnostics &diagnostics;
    vec<sptr<FunctionFrame>> function_stack;
};
#pragma once
#include "AST.hpp"
#include "Scope.hpp"
#include "Symbol.hpp"
#include <cstddef>

class SymbolTable {
public:
    SymbolTable();

    InsertResult declare(uptr<Symbol> symbol);
    LookupResult lookup(const std::string &name) const;

    void beginScope();
    void endScope();

    Scope &currentScope();
    const Scope &currentScope() const;

    std::size_t scopeDepth() const;

private:
    vec<sptr<Scope>> scopes_;
    vec<uptr<Symbol>> symbols_;
};
#include "SymbolTable.hpp"
#include "Diagnostics.hpp"
#include "Scope.hpp"
#include <cassert>

SymbolTable::SymbolTable() {
    beginScope();// global scope
}

void SymbolTable::beginScope() {
    scopes_.emplace_back(std::make_shared<Scope>(scopes_.empty() ? nullptr : scopes_.back()));
}

void SymbolTable::endScope() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

Scope &SymbolTable::currentScope() {
    assert(!scopes_.empty());
    return *scopes_.back();
}

const Scope &SymbolTable::currentScope() const {
    assert(!scopes_.empty());
    return *scopes_.back();
}

InsertResult SymbolTable::declare(uptr<Symbol> symbol) {
    if (!symbol) {
        return InsertResult::error();
    }
    Symbol *symPtr = symbol.get();
    // store the symbol to manage its lifetime(ownership)
    symbols_.emplace_back(std::move(symbol));
    return currentScope().declare(symPtr);
}

LookupResult SymbolTable::lookup(const std::string &name) const {
    return currentScope().lookup(name);
}

std::size_t SymbolTable::scopeDepth() const {
    return scopes_.size();
}
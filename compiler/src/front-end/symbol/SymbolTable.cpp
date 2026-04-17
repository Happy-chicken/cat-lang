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

LookupResult SymbolTable::lookup(const llvm::StringRef name) const {
  return currentScope().lookup(name);
}

bool SymbolTable::replaceSymbol(const llvm::StringRef name, uptr<Symbol> newSymbol) {
  if (!newSymbol) {
    return false;
  }
  // Look up the old symbol in current scope
  auto result = currentScope().lookup(name);
  if (!result.found()) {
    return false;
  }

  Symbol *oldSymPtr = result.symbol;
  Symbol *newSymPtr = newSymbol.get();
  string oldName = oldSymPtr->getName();
  // Find and replace the old symbol in the symbols_ vector
  for (auto &sym: symbols_) {
    if (sym.get() == oldSymPtr) {
      sym = std::move(newSymbol);
      break;
    }
  }
  // Replace the pointer in the current scope
  return currentScope().replace(oldName, newSymPtr);
}

std::size_t SymbolTable::scopeDepth() const {
  return scopes_.size();
}

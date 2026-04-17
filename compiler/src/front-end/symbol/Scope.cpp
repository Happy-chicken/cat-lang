#include "Scope.hpp"

Scope::Scope(sptr<Scope> parent_scope) : parent_scope(parent_scope) {}
Scope::Scope(sptr<Scope> parent_scope, llvm::StringMap<Symbol *> symbolTable)
    : parent_scope(parent_scope), symbolTable_(symbolTable) {}

// declare a symbol in the current scope
InsertResult Scope::declare(Symbol *symbol) {
  const string &name = symbol->getName();
  if (symbolTable_.find(name) != symbolTable_.end()) {
    return InsertResult::redeclared(symbolTable_[name]);
  }
  symbolTable_[name] = symbol;
  return InsertResult::ok(symbol);
}

// lookup a symbol only in the current scope
LookupResult Scope::lookupLocal(const llvm::StringRef name) const {
  auto it = symbolTable_.find(name);
  if (it != symbolTable_.end()) {
    return LookupResult::ok(it->second, shared_from_this());
  }
  return LookupResult::notFound();
}

// lookup a symbol in the current scope and parent scopes
LookupResult Scope::lookup(const llvm::StringRef name) const {
  auto current = shared_from_this();

  while (current != nullptr) {
    auto result = current->lookupLocal(name);
    if (result.found()) {
      return result;
    }
    current = current->getParentScope();
  }
  return LookupResult::notFound();
}

// replace a symbol in the current scope
bool Scope::replace(const llvm::StringRef name, Symbol *newSymbol) {
  auto it = symbolTable_.find(name);
  if (it == symbolTable_.end()) {
    return false;// Symbol not found in current scope
  }
  symbolTable_[name] = newSymbol;
  return true;
}

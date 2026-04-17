#pragma once
#include "AST.hpp"
#include "Scope.hpp"
#include "Symbol.hpp"
#include <cstddef>

class SymbolTable {
  public:
  SymbolTable();

  InsertResult declare(uptr<Symbol> symbol);
  LookupResult lookup(const llvm::StringRef name) const;
  bool replaceSymbol(const llvm::StringRef name, uptr<Symbol> newSymbol);

  void beginScope();
  void endScope();

  Scope &currentScope();
  const Scope &currentScope() const;

  std::size_t scopeDepth() const;

  void dump(std::ostream &out) const {
    for (std::size_t i = 0; i < symbols_.size(); ++i) {
      out << "Symbol " << i << ": ";
      symbols_[i]->dump(out);
      out << "\n";
    }
  }

  private:
  vec<sptr<Scope>> scopes_;
  vec<uptr<Symbol>> symbols_;
};

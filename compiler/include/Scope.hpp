#pragma once

#include "Diagnostics.hpp"
#include "Symbol.hpp"
#include <llvm-20/llvm/ADT/StringMap.h>
#include <llvm-20/llvm/ADT/StringRef.h>
#include <memory>
template<typename T>
using sptr = std::shared_ptr<T>;

class Scope : public std::enable_shared_from_this<Scope> {
  public:
  Scope() = default;
  explicit Scope(sptr<Scope> parent_scope);
  Scope(sptr<Scope> parent_scope, llvm::StringMap<Symbol *> symbolTable);

  InsertResult declare(Symbol *symbol);
  LookupResult lookup(const llvm::StringRef name) const;
  LookupResult lookupLocal(const llvm::StringRef name) const;
  bool replace(const llvm::StringRef name, Symbol *newSymbol);

  // TODO
  FuncSymbol *getEnclosingFunction() const { return nullptr; }
  sptr<Scope> getParentScope() const { return parent_scope; }

  const llvm::StringMap<Symbol *> &getSymbolTable() const {
    return symbolTable_;
  }

  void dump(std::ostream &out) const {
    auto *ps = shared_from_this().get();
    while (ps) {
      for (const auto &pair: symbolTable_) {
        out << pair.getKey().str() << ":";
        pair.second->dump(out);
        out << "\n";
      }
      ps = ps->getParentScope().get();
    }
  }

  private:
  sptr<Scope> parent_scope = nullptr;    // parent Scope link(parent_)
  llvm::StringMap<Symbol *> symbolTable_;// symbol table
};

#pragma once
#include "AST.hpp"
#include "Diagnostics.hpp"
#include "SemaType.hpp"
#include "Symbol.hpp"
#include "SymbolTable.hpp"
#include <cstddef>
class SemanticCtx {
  public:
  SemanticCtx(SymbolTable &sym_tab) : symbol_table(sym_tab) {}

  SymbolTable &getSymbolTable() { return symbol_table; }
  const SymbolTable &getSymbolTable() const { return symbol_table; }

  void beginScope() { symbol_table.beginScope(); }
  void endScope() { symbol_table.endScope(); }
  std::size_t scopeDepth() const { return symbol_table.scopeDepth(); }

  InsertResult declareSymbol(uptr<Symbol> sym, bool isRedeclared = false);
  LookupResult lookup(const string &name) const;
  LookupResult lookupLocalSymbol(const string &name) const;
  bool replaceSymbol(const string &name, uptr<Symbol> newSymbol);

  struct FunctionFrame {
    FuncSymbol *symbol = nullptr;
    SemaTypePtr return_type;
    bool is_procedure = false;
  };

  void enterFunction(sptr<FunctionFrame> frame);
  void leaveFunction();
  sptr<FunctionFrame> currentFunction();
  const sptr<FunctionFrame> currentFunction() const;
  void dumpSymbolTable(std::ostream &out) const {
    symbol_table.dump(out);
  }
  void dumpFuncFrames(std::ostream &out) const {
    out << "Function stack (depth " << function_stack.size() << "):\n";
    for (std::size_t i = 0; i < function_stack.size(); ++i) {
      auto &frame = function_stack[i];
      out << "  [" << i << "] ";
      if (frame && frame->symbol) {
        out << frame->symbol->getName() << "\n";
      } else {
        out << "<null>\n";
      }
    }
  }

  private:
  SymbolTable &symbol_table;
  vec<sptr<FunctionFrame>> function_stack;
};

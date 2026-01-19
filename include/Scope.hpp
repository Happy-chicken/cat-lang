#pragma once

#include "Diagnostics.hpp"
#include "Symbol.hpp"
#include "Token.hpp"
#include <memory>
#include <string>
#include <unordered_map>
template<typename T>
using sptr = std::shared_ptr<T>;
using std::string;
using std::unordered_map;

class Scope : public std::enable_shared_from_this<Scope> {
public:
    Scope() = default;
    explicit Scope(sptr<Scope> parent_scope);
    Scope(sptr<Scope> parent_scope, unordered_map<string, Symbol *> symbolTable);

    InsertResult declare(Symbol *symbol);
    LookupResult lookup(const string &name) const;
    LookupResult lookupLocal(const string &name) const;
    bool replace(const string &name, Symbol *newSymbol);

    // TODO
    FuncSymbol *getEnclosingFunction() const { return nullptr; }
    sptr<Scope> getParentScope() const { return parent_scope; }

    const unordered_map<string, Symbol *> &getSymbolTable() const {
        return symbolTable_;
    }

    void dump(std::ostream &out) const {
        auto *ps = shared_from_this().get();
        while (ps) {
            for (const auto &pair: symbolTable_) {
                out << pair.first << ":";
                pair.second->dump(out);
                out << "\n";
            }
            ps = ps->getParentScope().get();
        }
    }

private:
    sptr<Scope> parent_scope = nullptr;          // parent Scope link(parent_)
    unordered_map<string, Symbol *> symbolTable_;// symbol table
};
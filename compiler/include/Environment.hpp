#pragma once
#include "Symbol.hpp"
#include <llvm-20/llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <sstream>
#include <unordered_map>
template<typename T>
using sptr = std::shared_ptr<T>;
using std::unordered_map;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    using Env = sptr<Environment>;
    using ValueMap = unordered_map<const Symbol *, llvm::Value *>;
    using FuncMap = unordered_map<const FuncSymbol *, llvm::Function *>;
    explicit Environment(Env parent = nullptr) : parentEnv(std::move(parent)) {}
    Environment(Env parent, ValueMap valMap, FuncMap funcMap)
        : parentEnv(std::move(parent)),
          llvmValRecords(std::move(valMap)),
          llvmFuncRecords(std::move(funcMap)) {}

    void bind(Symbol *sym, llvm::Value *val) {
        if (sym) {
            llvmValRecords[sym] = val;
        }
    }
    void bindFunc(const FuncSymbol *sym, llvm::Function *func) {
        if (sym) {
            llvmFuncRecords[sym] = func;
        }
    }
    llvm::Value *lookup(Symbol *sym) {
        if (Env env = resolve(sym)) {
            return env->llvmValRecords[sym];
        }
        return nullptr;
    }
    llvm::Function *lookupFunc(FuncSymbol *sym) {
        if (Env env = resolveFunc(sym)) {
            return env->llvmFuncRecords[sym];
        }
        return nullptr;
    }
    string dump() const {
        std::stringstream sstream;
        for (auto &valRec: llvmValRecords) {
            sstream << valRec.first->getName() << ":" << valRec.second->getName().data();
        }
        for (auto &funRec: llvmFuncRecords) {
            sstream << funRec.first->getName() << ":" << funRec.second->getName().data();
        }
        return sstream.str();
    }

private:
    ValueMap llvmValRecords;
    FuncMap llvmFuncRecords;

    Env parentEnv;
    Env resolve(Symbol *sym) {
        if (llvmValRecords.find(sym) != llvmValRecords.end()) {
            return shared_from_this();
        }
        if (parentEnv) {
            return parentEnv->resolve(sym);
        }
        return nullptr;
    }
    Env resolveFunc(FuncSymbol *sym) {
        if (llvmFuncRecords.find(sym) != llvmFuncRecords.end()) {
            return shared_from_this();
        }
        if (parentEnv) {
            return parentEnv->resolveFunc(sym);
        }
        return nullptr;
    }
};

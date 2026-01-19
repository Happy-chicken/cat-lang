#pragma once

#include "AST.hpp"
#include "Location.hpp"
#include "SemaType.hpp"
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

class FuncSymbol;
class Symbol {
public:
    enum class SymKind {
        VAR,
        PARAM,
        FUNC,
        CLASS
    };

    enum class ParamPass {
        BY_VAL,
        BY_REF
    };

    virtual ~Symbol() = default;
    const std::string &getName() const;
    SymKind getKind() const;

    const SemaTypePtr &getType() const;
    Location getLocation() const;

    FuncSymbol *definingFunc() const;
    void setDefiningFunc(FuncSymbol *f);

    bool isVariable() const;
    bool isParameter() const;
    void markForwardDeclaration();
    void markDefined();
    bool isDefined() const;

    void dump(std::ostream &out) const;

protected:
    Symbol(std::string name, SymKind kind, SemaTypePtr type, Location loc);

private:
    std::string name_;
    SymKind kind_;
    SemaTypePtr type_;
    Location loc_;
    FuncSymbol *definingFunc_ = nullptr;
    bool isForward_ = false;
    bool isDefined_ = false;
};

class VarSymbol : public Symbol {
public:
    VarSymbol(std::string name, SemaTypePtr type, Location loc)
        : Symbol(std::move(name), SymKind::VAR, std::move(type), loc) {};
};

class ParamSymbol : public Symbol {
public:
    ParamSymbol(std::string name, SemaTypePtr type, ParamPass pass, Location loc)
        : Symbol(std::move(name), SymKind::PARAM, std::move(type), loc), pass_(pass) {};
    ParamPass getPass() const;

private:
    ParamPass pass_;
};

// Note: A FuncSymbol's type is its signature type (return type + parameter types)
class FuncSymbol : public Symbol {
public:
    FuncSymbol(std::string name, SemaTypePtr sigType, bool isProc, Location loc)
        : Symbol(std::move(name), SymKind::FUNC, std::move(sigType), loc), isProcedure_(isProc) {};

    void addParam(ParamSymbol *param);
    const std::vector<ParamSymbol *> &getParams() const;
    bool isProcedure() const;
    void clearParams();

private:
    std::vector<ParamSymbol *> params_;
    bool isProcedure_;
};

class ClassSymbol : public Symbol {
public:
    using MemberMap = std::unordered_map<std::string, Symbol *>;
    ClassSymbol(std::string name, SemaTypePtr classType, Location loc)
        : Symbol(std::move(name), SymKind::CLASS, std::move(classType), loc) {};
    void addField(VarSymbol *field) {
        fields_.push_back(field);
        memberMap_[field->getName()] = field;
    }
    void addMethod(FuncSymbol *method) {
        methods_.push_back(method);
        memberMap_[method->getName()] = method;
    }
    const vec<VarSymbol *> &getFields() const { return fields_; }
    const vec<FuncSymbol *> &getMethods() const { return methods_; }
    void clearAll() {
        fields_.clear();
        methods_.clear();
        memberMap_.clear();
    }

private:
    vec<VarSymbol *> fields_;
    vec<FuncSymbol *> methods_;
    MemberMap memberMap_;
    std::optional<std::string> baseClassName_;
};

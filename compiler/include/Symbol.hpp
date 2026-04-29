#pragma once

#include "AST.hpp"
#include "Location.hpp"
#include "SemaType.hpp"
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
template<typename T>
using vec = std::vector<T>;
class FuncSymbol;
class ClassSymbol;
class Symbol {
  public:
  enum class SymKind {
    VAR,
    PARAM,
    FUNC,
    CLASS,
    METHOD,// Function defined in a class
    FIELD  // Variable defined in a class
  };

  enum class ParamPass {
    BY_VAL,
    BY_REF
  };

  virtual ~Symbol() = default;
  const std::string &getName() const;
  void setName(const std::string &newname) { name_ = newname; }
  SymKind getKind() const;

  const SemaTypePtr &getType() const;
  Location getLocation() const;

  FuncSymbol *definingFunc() const;
  void setDefiningFunc(FuncSymbol *f);

  ClassSymbol *definingClass() const;
  void setDefiningClass(ClassSymbol *c);

  bool isVariable() const;
  bool isParameter() const;
  bool isMethod() const;
  bool isField() const;
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
  ClassSymbol *definingClass_ = nullptr;
  bool isForward_ = false;
  bool isDefined_ = false;
};

class VarSymbol : public Symbol {
  public:
  VarSymbol(std::string name, SemaTypePtr type, Location loc)
      : Symbol(std::move(name), SymKind::VAR, std::move(type), loc) {};

  protected:
  VarSymbol(std::string name, SymKind kind, SemaTypePtr type, Location loc)
      : Symbol(name, kind, std::move(type), loc) {}
};

// Field is a variable defined in a class
class FieldSymbol : public VarSymbol {
  public:
  FieldSymbol(std::string name, SemaTypePtr type, Location loc)
      : VarSymbol(std::move(name), SymKind::FIELD, std::move(type), loc) {};
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
  FuncSymbol(std::string name, SemaTypePtr sigType, bool isProc, bool isVariadic, Location loc)
      : Symbol(std::move(name), SymKind::FUNC, std::move(sigType), loc), isProcedure_(isProc), isVariadic_(isVariadic) {};

  void addParam(ParamSymbol *param);
  const std::vector<ParamSymbol *> &getParams() const;
  bool isProcedure() const;
  bool isVariadic() const;
  bool isBuiltin() const;
  void setBuiltin(bool is_builtin);
  void clearParams();

  protected:
  // Protected constructor for derived classes (e.g., MethodSymbol)
  FuncSymbol(std::string name, SymKind kind, SemaTypePtr sigType, bool isProc, Location loc)
      : Symbol(std::move(name), kind, std::move(sigType), loc), isProcedure_(isProc), isVariadic_(false) {};

  private:
  std::vector<ParamSymbol *> params_;
  bool isProcedure_;
  bool isVariadic_;
  bool isBuiltin_{false};
};

// Method is a function defined in a class (has implicit 'this' parameter)
class MethodSymbol : public FuncSymbol {
  public:
  MethodSymbol(std::string name, SemaTypePtr sigType, bool isProc, Location loc)
      : FuncSymbol(std::move(name), SymKind::METHOD, sigType, isProc, loc) {}
};

class ClassSymbol : public Symbol {
  public:
  using MemberMap = std::unordered_map<std::string, Symbol *>;
  ClassSymbol(std::string name, SemaTypePtr classType, Location loc)
      : Symbol(std::move(name), SymKind::CLASS, std::move(classType), loc) {};
  void addField(FieldSymbol *field) {
    fields_.push_back(field);
    memberMap_[field->getName()] = field;
  }
  void addMethod(MethodSymbol *method) {
    methods_.push_back(method);
    memberMap_[method->getName()] = method;
  }
  const vec<FieldSymbol *> &getFields() const { return fields_; }
  const vec<MethodSymbol *> &getMethods() const { return methods_; }
  void clearAll() {
    fields_.clear();
    methods_.clear();
    memberMap_.clear();
  }

  private:
  vec<FieldSymbol *> fields_;
  vec<MethodSymbol *> methods_;
  MemberMap memberMap_;
  std::optional<std::string> baseClassName_;
};

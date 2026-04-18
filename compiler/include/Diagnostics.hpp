#pragma once
#include "Location.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
template<typename T>
using sptr = std::shared_ptr<T>;

class Symbol;
class Scope;

struct InsertResult {
  enum class Status {
    SUCCESS,
    REDECLARED,
    ERROR
  };

  Symbol *symbol = nullptr;
  Status status = Status::ERROR;

  bool isSuccess() const { return status == Status::SUCCESS; }
  bool isRedeclared() const { return status == Status::REDECLARED; }
  bool isError() const { return status == Status::ERROR; }
  bool isFailed() const { return !isSuccess(); }

  // factory methods
  static InsertResult ok(Symbol *sym) {
    return {sym, Status::SUCCESS};
  }
  static InsertResult redeclared(Symbol *sym) {
    return {sym, Status::REDECLARED};
  }
  static InsertResult error() {
    return {nullptr, Status::ERROR};
  }

  explicit operator bool() const {
    return isSuccess();
  }
};

struct LookupResult {
  Symbol *symbol = nullptr;
  sptr<const Scope> owner = nullptr;

  bool found() const { return symbol != nullptr; }

  // bool isLocal() const;
  // bool isGlobal() const;

  // factory methods
  static LookupResult ok(Symbol *sym, sptr<const Scope> owner) {
    return {sym, owner};
  }
  static LookupResult notFound() {
    return {nullptr, nullptr};
  }

  explicit operator bool() const {
    return found();
  }
};

class Diagnostics {
  public:
  using ptr = sptr<Diagnostics>;

  enum class Severity {
    Info,
    Warning,
    Error
  };

  enum class Phase {
    Lexing,
    Parsing,
    SemanticAnalysis,
  };

  struct Entry {
    Severity severity;
    Phase phase;
    Location loc;
    std::string message;
  };

  void report(Severity severity, Phase phase, const Location &location, const std::string &message);
  void report(Severity severity, int line, int col, const char *fmt, ...);
  void printAll() const;
  void clear();

  const std::vector<Entry> &getEntries() const;
  std::size_t errorCount() const;
  std::size_t warningCount() const;
  bool hasErrors() const;


  private:
  std::vector<Entry> entries;
  std::string fileName;
  std::size_t error_count = 0;
  std::size_t warning_count = 0;
};

template<typename T>
class Singleton {
  public:
  static T *getInstance() {
    static T instance;
    return &instance;
  }
};
template<typename T>
class SingletonPtr {
  public:
  static sptr<T> getInstance() {
    static sptr<T> instance(new T);
    return instance;
  }
};

using Diag = Singleton<Diagnostics>;

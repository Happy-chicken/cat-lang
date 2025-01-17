#ifndef ENVIRONMENT_HPP_
#define ENVIRONMENT_HPP_

#include "Logger.hpp"
#include "Token.hpp"
#include <llvm/IR/Use.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

using std::shared_ptr;
using std::string;
using std::unordered_map;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment() = default;
    explicit Environment(shared_ptr<Environment> enclosing);
    Environment(shared_ptr<Environment> enclosing, std::map<string, llvm::Value *> llvmRecord) : enclosing(enclosing), llvmRecord_(llvmRecord){};

    llvm::Value *define(string name, llvm::Value *value);

    llvm::Value *lookup(const string &name);
    llvm::Value *lookup(const Token &name);

    shared_ptr<Environment> enclosing;// parent environment link(parent_)

private:
    unordered_map<string, Object> values;
    std::map<string, llvm::Value *> llvmRecord_;

    std::shared_ptr<Environment> resolve(const string &name) {
        if (llvmRecord_.count(name) != 0) {
            return shared_from_this();
        }
        if (enclosing == nullptr) {
            Error::ErrorLogMessage() << "Undefined variable '" << name << "'.";
        }
        return enclosing->resolve(name);
    }

    std::shared_ptr<Environment> resolve(const Token &name) {
        if (llvmRecord_.count(name.lexeme) != 0) {
            return shared_from_this();
        }
        if (enclosing == nullptr) {
            string msg = string("[CyLang]: Undefined variable '") + name.lexeme + "'.";
            Error::addError(name, msg);
            throw std::runtime_error(to_string(name.line) + " at '" + name.lexeme + "'" + msg);
        }
        return enclosing->resolve(name);
    }
};

#endif// ENVIRONMENT_HPP_


#include "../include/Environment.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

using std::string;

Environment::Environment(shared_ptr<Environment> enclosing_) : enclosing(enclosing_) {}

// llvm environment
llvm::Value *Environment::define(string name, llvm::Value *value) {
    llvmRecord_[name] = value;
    return value;
}


llvm::Value *Environment::lookup(const string &name) {
    return resolve(name)->llvmRecord_[name];
}

llvm::Value *Environment::lookup(const Token &name) {
    return resolve(name)->llvmRecord_[name.lexeme];
}

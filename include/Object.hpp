#pragma once

#include "Value.hpp"
#include <llvm-14/llvm/IR/Value.h>
#include <memory>
#include <string>
#include <variant>
namespace CatVM {
    struct CodeObject;
}
using std::string;
using std::to_string;
//monostate non-valid state in variant in c++17
using Objects = std::variant<std::string, int, double, bool, std::monostate>;
class Object {
public:
    Objects data;

    llvm::Value *llvmValue;  // used for llvm value
    CatVM::Value *catvmValue;// used for vm code object


public:
    static Object make_llvmval_obj(llvm::Value *llvmValue_) {
        Object obj;
        obj.llvmValue = llvmValue_;
        return obj;
    }

    static Object make_catvmval_obj(CatVM::Value *catvmValue_) {
        Object obj;
        obj.catvmValue = catvmValue_;
        return obj;
    }

    static Object make_none_obj() {
        Object none_obj;
        none_obj.data = std::monostate{};
        return none_obj;
    }

    template<typename T>
    static Object make_obj(T data_) {
        Object obj;
        obj.data = data_;
        return obj;
    }

    string toString() {
        switch (data.index()) {
            case 0:
                return std::get<string>(data);
            case 1:
                return to_string(std::get<int>(data));
            case 2:
                return to_string(std::get<double>(data));
            case 3:
                return std::get<bool>(data) ? "true" : "false";
            case 4:
                return "nil";
            default:
                return "Unknown type";
        }
    }
};
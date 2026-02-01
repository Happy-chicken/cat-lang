#include "Dialect/Cat/IR/CatDialect.h"

#include "llvm/Support/raw_ostream.h"
#define FIX
#include "Dialect/Cat/IR/CatDialect.cpp.inc"
#undef FIX

namespace mlir::cat {
    // 实现方言的初始化方法
    void CatDialect::initialize() {
        llvm::outs() << "initializing " << getDialectNamespace() << "\n";
        registerType();
    }

    // 实现方言的析构函数
    CatDialect::~CatDialect() {
        llvm::outs() << "destroying " << getDialectNamespace() << "\n";
    }

    // 实现在extraClassDeclaration 声明当中生命的方法。
    void CatDialect::sayHello() {
        llvm::outs() << "Hello in " << getDialectNamespace() << "\n";
    }

}// namespace mlir::cat

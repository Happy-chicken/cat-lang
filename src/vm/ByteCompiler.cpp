#include "ByteCompiler.hpp"
#include "Expr.hpp"
#include "Logger.hpp"
#include "Object.hpp"
#include "OpCode.hpp"
#include "Stmt.hpp"
#include "Value.hpp"
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#define ALLOC_CONST(checker, astype, allocator, value)         \
    do {                                                       \
        for (auto i = 0; i < codeobj->constants.size(); i++) { \
            if (!checker(codeobj->constants[i])) {             \
                continue;                                      \
            }                                                  \
            if (astype(codeobj->constants[i]) == value) {      \
                return i;                                      \
            }                                                  \
        }                                                      \
        codeobj->constants.push_back(allocator(value));        \
    } while (false)
// --------------------------------------
// core
// --------------------------------------
void ByteCompiler::emit(uint8_t opcode) {
    codeobj->bytecodes.push_back(opcode);
}
// --------------------------------------
// helper function
// --------------------------------------
size_t ByteCompiler::ConstIndex(bool value) {
    ALLOC_CONST(IS_BOOL, AS_BOOL, BOOL, value);
    return codeobj->constants.size() - 1;
}

size_t ByteCompiler::ConstIndex(int value) {
    ALLOC_CONST(IS_INT, AS_INT, INT, value);
    return codeobj->constants.size() - 1;
}

size_t ByteCompiler::ConstIndex(double value) {
    ALLOC_CONST(IS_DOUBLE, AS_DOUBLE, DOUBLE, value);
    return codeobj->constants.size() - 1;
}

size_t ByteCompiler::StringIndex(const string &value) {
    ALLOC_CONST(IS_STRING, AS_CPP_STRING, ALLOC_STRING, value);
    return codeobj->constants.size() - 1;
}
// --------------------------------------
// visitor
// --------------------------------------
Object ByteCompiler::visitLiteralExpr(shared_ptr<Literal<Object>> expr) {
    if (expr->value.data.index() == 0) {
        emit(OP_CONST);
        emit(StringIndex(std::get<string>(expr->value.data)));
    } else if (expr->value.data.index() == 1) {
        emit(OP_CONST);
        emit(ConstIndex(std::get<int>(expr->value.data)));
    } else if (expr->value.data.index() == 2) {
        emit(OP_CONST);
        emit(ConstIndex(std::get<double>(expr->value.data)));
    } else if (expr->value.data.index() == 3) {
        emit(OP_CONST);
        emit(ConstIndex(std::get<bool>(expr->value.data)));
    }
    return Object::make_none_obj();
}
Object ByteCompiler::visitAssignExpr(shared_ptr<Assign<Object>> expr) {
    int a = 1 + 1;
    return Object::make_none_obj();
}
Object ByteCompiler::visitBinaryExpr(shared_ptr<Binary<Object>> expr) {
    evaluate(expr->left);
    evaluate(expr->right);
    switch (expr->operation.type) {
        case TokenType::PLUS:
            emit(OP_ADD);
            break;
        case TokenType::MINUS:
            emit(OP_SUB);
            break;
        case TokenType::STAR:
            emit(OP_MUL);
            break;
        case TokenType::SLASH:
            emit(OP_DIV);
            break;
        // TODO: more and more binary operation
        default:
            Error::addError(expr->operation, "[CatVM]: Unknown binary operator.");
            break;
    }
    return Object::make_none_obj();
}
Object ByteCompiler::visitGroupingExpr(shared_ptr<Grouping<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitUnaryExpr(shared_ptr<Unary<Object>> expr) {
    auto op = expr->operation;
    evaluate(expr->right);
    switch (expr->operation.type) {
        case TokenType::BANG:
            emit(OP_NOT);
            break;
        case TokenType::MINUS:
            // TODO
            emit(OP_SUB);
            break;
        default:
            Error::addError(expr->operation, "[CatVM]: Unknown logical operator.");
            break;
    }
    return Object::make_none_obj();
}
Object ByteCompiler::visitVariableExpr(shared_ptr<Variable<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitLogicalExpr(shared_ptr<Logical<Object>> expr) {
    auto left = evaluate(expr->left);
    auto right = evaluate(expr->right);
    switch (expr->operation.type) {
        case TokenType::AND:
            emit(OP_AND);
            break;
        case TokenType::OR:
            emit(OP_OR);
            break;
        default:
            Error::addError(expr->operation, "[CatVM]: Unknown logical operator.");
            break;
    }
    return Object::make_none_obj();
}
// Object visitIncrementExpr(shared_ptr<Increment<Object>> expr){Object::make_none_obj();}
// Object visitDecrementExpr(shared_ptr<Decrement<Object>> expr){Object::make_none_obj();}
Object ByteCompiler::visitListExpr(shared_ptr<List<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitSubscriptExpr(shared_ptr<Subscript<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitCallExpr(shared_ptr<Call<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitGetExpr(shared_ptr<Get<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitSetExpr(shared_ptr<Set<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitSelfExpr(shared_ptr<Self<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitSuperExpr(shared_ptr<Super<Object>> expr) {
    return Object::make_none_obj();
}

void ByteCompiler::visitExpressionStmt(const Expression &stmt) {
    evaluate(stmt.expression);
    return;
}
void ByteCompiler::visitPrintStmt(const Print &stmt) {
    auto expr = std::dynamic_pointer_cast<Call<Object>>(stmt.expression);


    for (auto &arg: expr->arguments) {
        evaluate(arg);
    }
    emit(OP_PRINT);
    emit(ConstIndex(int(expr->arguments.size())));
}
void ByteCompiler::visitVarStmt(const Var &stmt) { return; }
void ByteCompiler::visitBlockStmt(const Block &stmt) { return; }
void ByteCompiler::visitClassStmt(const Class &stmt) {
    return;
}
void ByteCompiler::visitIfStmt(const If &stmt) { return; }
void ByteCompiler::visitWhileStmt(const While &stmt) { return; }
void ByteCompiler::visitFunctionStmt(shared_ptr<Function> stmt) { return; }
void ByteCompiler::visitReturnStmt(const Return &stmt) { return; }
void ByteCompiler::visitBreakStmt(const Break &stmt) { return; }
void ByteCompiler::visitContinueStmt(const Continue &stmt) { return; }
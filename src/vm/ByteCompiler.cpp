#include "ByteCompiler.hpp"
#include "Object.hpp"
#include "OpCode.hpp"
#include "Stmt.hpp"
#include <cstdint>
#include <variant>
// --------------------------------------
// core
// --------------------------------------
void ByteCompiler::emit(uint8_t opcode) {
    codeobj->bytecodes.push_back(opcode);
}
// --------------------------------------
// helper function
// --------------------------------------
size_t ByteCompiler::ConstIndex(int value) {
    for (auto i = 0; i < codeobj->constants.size(); i++) {
        if (!IS_INT(codeobj->constants[i])) {
            continue;
        }
        if (AS_INT(codeobj->constants[i]) == value) {
            return i;
        }
    }
    codeobj->constants.push_back(INT(value));
    return codeobj->constants.size() - 1;
}

size_t ByteCompiler::ConstIndex(double value) {
    for (auto i = 0; i < codeobj->constants.size(); i++) {
        if (!IS_DOUBLE(codeobj->constants[i])) {
            continue;
        }
        if (AS_DOUBLE(codeobj->constants[i]) == value) {
            return i;
        }
    }
    codeobj->constants.push_back(DOUBLE(value));
    return codeobj->constants.size() - 1;
}

size_t ByteCompiler::StringIndex(const string &value) {
    for (auto i = 0; i < codeobj->bytecodes.size(); i++) {
        if (!IS_STRING(codeobj->constants[i])) {
            continue;
        }
        if (AS_CPP_STRING(codeobj->constants[i]) == value) {
            return i;
        }
    }
    codeobj->constants.push_back(ALLOC_STRING(value));
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
    }
    return Object::make_none_obj();
}
Object ByteCompiler::visitAssignExpr(shared_ptr<Assign<Object>> expr) {
    int a = 1 + 1;
    return Object::make_none_obj();
}
Object ByteCompiler::visitBinaryExpr(shared_ptr<Binary<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitGroupingExpr(shared_ptr<Grouping<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitUnaryExpr(shared_ptr<Unary<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitVariableExpr(shared_ptr<Variable<Object>> expr) {
    return Object::make_none_obj();
}
Object ByteCompiler::visitLogicalExpr(shared_ptr<Logical<Object>> expr) {
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
void ByteCompiler::visitPrintStmt(const Print &stmt) { return; }
void ByteCompiler::visitVarStmt(const Var &stmt) { return; }
void ByteCompiler::visitBlockStmt(const Block &stmt) { return; }
void ByteCompiler::visitClassStmt(const Class &stmt) { return; }
void ByteCompiler::visitIfStmt(const If &stmt) { return; }
void ByteCompiler::visitWhileStmt(const While &stmt) { return; }
void ByteCompiler::visitFunctionStmt(shared_ptr<Function> stmt) { return; }
void ByteCompiler::visitReturnStmt(const Return &stmt) { return; }
void ByteCompiler::visitBreakStmt(const Break &stmt) { return; }
void ByteCompiler::visitContinueStmt(const Continue &stmt) { return; }
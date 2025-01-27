#pragma once
#include "Expr.hpp"
#include "OpCode.hpp"
#include "Stmt.hpp"
#include "Value.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
class ByteCompiler : public Visitor<Object>,
                     public Visitor_Stmt,
                     public std::enable_shared_from_this<ByteCompiler> {
public:
    ByteCompiler() : codeobj(nullptr){};
    CatVM::CodeObject *compile(vector<shared_ptr<Stmt>> &statements) {// compile to bytecode

        // CatVM::Value c = ALLOC_CODE("main");
        // codeobj = static_cast<CatVM::CodeObject *>(c.as.object);
        codeobj = AS_CODE(ALLOC_CODE("main"));
        assert(codeobj != nullptr);
        for (auto stmt: statements) {
            execute(stmt);
        }
        emit(OP_HALT);
        return codeobj;
    }


private:
    CatVM::CodeObject *codeobj;
    static std::map<std::string, uint8_t> compareOpMap;

private:
    // void executeBlock(vector<shared_ptr<Stmt>> statements, Env env);
    void execute(shared_ptr<Stmt> stmt) {
        stmt->accept(shared_from_this());
    }
    CatVM::Value *evaluate(shared_ptr<Expr<Object>> expr) {
        return expr->accept(shared_from_this()).catvmValue;
    }

    //----------------------------------------------
    // core
    // --------------------------------------------
    void emit(uint8_t opcode);
    //----------------------------------------------
    // helper function
    // --------------------------------------------
    void patchJump(size_t offset, uint16_t jumpAddr);
    void writeByteAtOffset(size_t offset, uint8_t value);
    size_t ConstIndex(bool value);
    size_t ConstIndex(int value);// get the index of the constant in constants pool
    size_t ConstIndex(double value);
    size_t StringIndex(const string &value);
    //----------------------------------------------
    // visitor
    // --------------------------------------------
    // IR generator visitor
    Object visitLiteralExpr(shared_ptr<Literal<Object>> expr);
    Object visitAssignExpr(shared_ptr<Assign<Object>> expr);
    Object visitBinaryExpr(shared_ptr<Binary<Object>> expr);
    Object visitGroupingExpr(shared_ptr<Grouping<Object>> expr);
    Object visitUnaryExpr(shared_ptr<Unary<Object>> expr);
    Object visitVariableExpr(shared_ptr<Variable<Object>> expr);
    Object visitLogicalExpr(shared_ptr<Logical<Object>> expr);
    // Object visitIncrementExpr(shared_ptr<Increment<Object>> expr);
    // Object visitDecrementExpr(shared_ptr<Decrement<Object>> expr);
    Object visitListExpr(shared_ptr<List<Object>> expr);
    Object visitSubscriptExpr(shared_ptr<Subscript<Object>> expr);
    Object visitCallExpr(shared_ptr<Call<Object>> expr);
    Object visitGetExpr(shared_ptr<Get<Object>> expr);
    Object visitSetExpr(shared_ptr<Set<Object>> expr);
    Object visitSelfExpr(shared_ptr<Self<Object>> expr);
    Object visitSuperExpr(shared_ptr<Super<Object>> expr);

    void visitExpressionStmt(const Expression &stmt);
    void visitPrintStmt(const Print &stmt);
    void visitVarStmt(const Var &stmt);
    void visitBlockStmt(const Block &stmt);
    void visitClassStmt(const Class &stmt);
    void visitIfStmt(const If &stmt);
    void visitWhileStmt(const While &stmt);
    void visitFunctionStmt(shared_ptr<Function> stmt);
    void visitReturnStmt(const Return &stmt);
    void visitBreakStmt(const Break &stmt);
    void visitContinueStmt(const Continue &stmt);
};
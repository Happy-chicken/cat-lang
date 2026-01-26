#pragma once
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "Location.hpp"
#include "Operator.hpp"
#include "SemaType.hpp"
#include "Symbol.hpp"
#include "Types.hpp"

using std::make_unique;
using std::move;
using std::optional;
using std::string;
template<class T>
using uptr = std::unique_ptr<T>;
template<class T>
using vec = std::vector<T>;

class AstVisitor;
class Symbol;
class FuncSymbol;
class VarSymbol;
class MethodSymbol;

// Base AST Node class
class ASTNode {
public:
    Location loc;

public:
    explicit ASTNode(Location loc);
    virtual ~ASTNode() = 0;
    virtual void accept(AstVisitor &v) = 0;
    virtual void print(std::ostream &out) const = 0;
};

// Overload << operator for printing AST nodes
inline std::ostream &operator<<(std::ostream &out, const ASTNode &node) {
    node.print(out);
    return out;
}

/* Base classes in the hierarchy, directly derived from ASTNode 
 * Each node class stores certain attributes. The pointer attributes (uptr=unique_ptr)
 * are the actual children nodes of a node in the AST. The other are just attributes
 * that uniquely define an AST node.
 * */

// Expression nodes
class Expr : public ASTNode {
public:
    explicit Expr(Location loc);
    virtual ~Expr();
    virtual void print(std::ostream &out) const override = 0;
    SemaTypePtr type() const;
    void setType(SemaTypePtr type);
    bool isLValue() const;
    void setLValue(bool v);
    bool isAssignable() const;
    void setAssignable(bool v);
    bool isConstExpr() const;
    void setConstExpr(bool v);

private:
    SemaTypePtr resolvedType_;
    bool isLValue_ = false;
    bool assignable_ = false;
    bool constExpr_ = false;
};

// Statements
class Stmt : public ASTNode {
public:
    explicit Stmt(Location l);
    virtual ~Stmt() override;
    virtual void print(std::ostream &out) const override = 0;
};

// L-values - the left part of an assignment statement
class Lval : public ASTNode {
public:
    explicit Lval(Location l);
    virtual ~Lval() override;
    virtual void print(std::ostream &out) const override = 0;
    SemaTypePtr type() const;
    void setType(SemaTypePtr type);
    bool isAssignable() const;
    void setAssignable(bool v);

private:
    SemaTypePtr resolvedType_;
    bool assignable_ = true;
};

// R-Values are expressions - the right part of an assignment statement
class Rval : public Expr {
public:
    explicit Rval(Location l);
    virtual ~Rval() override = default;
    virtual void print(std::ostream &out) const override = 0;
};

// Types
class Type : public ASTNode {
protected:
    DataType::DataType base;
    vec<std::optional<int>> dims;// dimensions for array types
    void printTypeDetails(std::ostream &out) const;
    std::optional<string> type_name;

public:
    Type(Location l, DataType::DataType b, vec<std::optional<int>> d = {});
    virtual ~Type() override = default;
    DataType::DataType data_type() const;
    void setTypeName(const string &name);
    const optional<string> &getName() const;
    const vec<std::optional<int>> &dimensions() const;
    void accept(AstVisitor &v) override;
    virtual void print(std::ostream &out) const override;
};

class FuncParameterType : public Type {
public:
    FuncParameterType(Location l, bool ref, DataType::DataType type);
    FuncParameterType(Location l, bool ref, DataType::DataType type, vec<std::optional<int>> d);

    bool isByRef() const;
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;

private:
    bool by_ref;
};

// Blocks
class Block : public ASTNode {
public:
    Block(Location l, vec<uptr<Stmt>> stmts);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    const vec<uptr<Stmt>> &statementsList() const { return statements; }

private:
    vec<uptr<Stmt>> statements;
};

// Definitions
class Def : public ASTNode {
public:
    explicit Def(Location l);
    void print(std::ostream &out) const override;
};

// Program root node
class Program : public ASTNode {
public:
    Program(Location l, vec<uptr<ASTNode>> defs = {});
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    const vec<uptr<ASTNode>> &getDefs() const { return defs; }

private:
    vec<uptr<ASTNode>> defs;
};

// ===== High-level program and definition nodes =====

class FuncParameterDef : public Def {
public:
    FuncParameterDef(Location l, vec<string> names, uptr<FuncParameterType> t);
    void accept(AstVisitor &v) override;
    const vec<string> &names() const;
    FuncParameterType *parameterType() {
        return type.get();
    }
    const FuncParameterType *parameterType() const;
    void print(std::ostream &out) const override;

private:
    vec<string> identifiers;
    uptr<FuncParameterType> type;
};

class Header : public Def {
public:
    Header(Location l, string n, optional<DataType::DataType> r, vec<uptr<FuncParameterDef>> p);

    void accept(AstVisitor &v) override;
    const string &identifier() const;
    const vec<uptr<FuncParameterDef>> &parameters() const;
    optional<DataType::DataType> returnType() const;
    FuncSymbol *symbol() const { return symbol_; }
    void setSymbol(FuncSymbol *sym) { symbol_ = sym; }
    void print(std::ostream &out) const override;

private:
    string name;
    optional<DataType::DataType> return_type;
    vec<uptr<FuncParameterDef>> params;
    // function symbol initialized with header
    FuncSymbol *symbol_ = nullptr;
};

class VarDef : public Stmt {
private:
    vec<string> names;
    uptr<Type> declared_type;
    vec<VarSymbol *> symbols_;
    std::optional<uptr<Expr>> init_expr_;

public:
    VarDef(Location l, vec<string> ids, uptr<Type> t, optional<uptr<Expr>> init = std::nullopt);

    void accept(AstVisitor &v) override;
    vec<VarSymbol *> &symbols() { return symbols_; }
    const vec<VarSymbol *> &symbols() const { return symbols_; }
    const vec<string> &identifiers() const { return names; }
    const optional<uptr<Expr>> &initExpr() const { return init_expr_; }
    Type *declaredType() { return declared_type.get(); }
    const Type *declaredType() const { return declared_type.get(); }
    void print(std::ostream &out) const override;
};

class FuncDecl : public Def {
public:
    explicit FuncDecl(Location l, uptr<Header> h);

    void accept(AstVisitor &v) override;
    Header *funcHeader() const { return header.get(); }
    void print(std::ostream &out) const override;

private:
    uptr<Header> header;
};

class FuncDef : public Stmt {
public:
    FuncDef(Location l, uptr<Header> h, uptr<Block> b);

    void accept(AstVisitor &v) override;
    Header *funcHeader() const { return header.get(); }
    Block *funcBody() const { return body.get(); }
    void print(std::ostream &out) const override;
    bool isEntrypoint();
    bool isMethod();
    void setEntrypoint(bool cond);
    void setIsMethod(bool cond);

private:
    uptr<Header> header;
    uptr<Block> body;
    bool isEntrypoint_;
    bool isMethod_;
};

class ClassDef : public Def {
public:
    ClassDef(Location l, string n, vec<uptr<VarDef>> f, vec<uptr<FuncDef>> m);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    const string &identifier() const { return name; }
    const vec<uptr<VarDef>> &fieldList() const { return fields; }
    const vec<uptr<FuncDef>> &methodList() const { return methods; }

private:
    string name;// class name
    vec<uptr<VarDef>> fields;
    vec<uptr<FuncDef>> methods;
};
// ===== Blocks and statements =====
class ExpressionStmt : public Stmt {
public:
    explicit ExpressionStmt(Location loc, uptr<Expr> expr);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    Expr *getExpr() const { return expression.get(); }

private:
    uptr<Expr> expression;
};

class SkipStmt : public Stmt {
public:
    explicit SkipStmt(Location l);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
};

class ExitStmt : public Stmt {
public:
    explicit ExitStmt(Location l);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
};

class AssignStmt : public Stmt {
public:
    AssignStmt(Location l, uptr<Lval> left, uptr<Expr> right);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Lval *left() const { return lhs.get(); }
    Expr *right() const { return rhs.get(); }

private:
    uptr<Lval> lhs;
    uptr<Expr> rhs;
};

class ReturnStmt : public Stmt {
public:
    ReturnStmt(Location l, uptr<Expr> expr);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Expr *returnValue() const { return value.get(); }

private:
    uptr<Expr> value;
};

class ProcCall : public Stmt {
public:
    ProcCall(Location l, string id, vec<uptr<Expr>> a);
    FuncSymbol *funcSymbol() const { return symbol_; }
    void setFuncSymbol(FuncSymbol *sym) { symbol_ = sym; }
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    const string &identifier() const { return name; }
    const vec<uptr<Expr>> &arguments() const { return args; }

private:
    string name;
    vec<uptr<Expr>> args;
    // store associated symbol (callee)
    FuncSymbol *symbol_ = nullptr;
};

class BreakStmt : public Stmt {
public:
    BreakStmt(Location l, optional<string> lbl);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    const optional<string> &loopLabel() const { return label; }

private:
    optional<string> label;
};

class ContinueStmt : public Stmt {
public:
    ContinueStmt(Location l, optional<string> lbl);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    const optional<string> &loopLabel() const { return label; }

private:
    optional<string> label;
};

// need to forward declare Cond for IfStmt
class Cond;
class IfStmt : public Stmt {
public:
    IfStmt(Location l, uptr<Cond> cond, uptr<Block> then_block, vec<std::pair<uptr<Cond>, uptr<Block>>> elifs, std::optional<uptr<Block>> else_block);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Cond *conditionExpr() const { return condition.get(); }
    Block *thenBlock() const { return then_branch.get(); }
    const vec<std::pair<uptr<Cond>, uptr<Block>>> &elifs() const { return elif_branches; }
    Block *elseBlock() const { return else_branch ? else_branch->get() : nullptr; }

private:
    uptr<Cond> condition;
    uptr<Block> then_branch;
    vec<std::pair<uptr<Cond>, uptr<Block>>> elif_branches;
    optional<uptr<Block>> else_branch;
};

class LoopStmt : public Stmt {
private:
    uptr<Cond> condition;
    uptr<Block> body;

public:
    LoopStmt(Location l, uptr<Cond> cond, uptr<Block> blk);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Cond *conditionExpr() const { return condition.get(); }
    Block *loopBody() const { return body.get(); }
};

// ===== L-values =====

class IdLVal : public Lval {
public:
    IdLVal(Location l, string id);
    Symbol *symbol() const { return symbol_; }
    void setSymbol(Symbol *sym) { symbol_ = sym; }
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    const string &identifier() const { return name; }

private:
    string name;
    // store associated symbol
    Symbol *symbol_ = nullptr;
};

class StringLiteralLVal : public Lval {
public:
    StringLiteralLVal(Location l, string v);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    const string &literal() const { return value; }

private:
    string value;
};

class IndexLVal : public Lval {
public:
    IndexLVal(Location l, uptr<Lval> b, uptr<Expr> idx);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Lval *baseExpr() const { return base.get(); }
    Expr *indexExpr() const { return index.get(); }

private:
    uptr<Lval> base;
    uptr<Expr> index;
};

// Member access as Lval (e.g., a.b on the left side of assignment)
class MemberAccessLVal : public Lval {
public:
    MemberAccessLVal(Location l, uptr<Expr> obj, string member);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Expr *object() const { return object_.get(); }
    const string &memberName() const { return member_; }
    Symbol *memberSymbol() const { return memberSymbol_; }
    void setMemberSymbol(Symbol *sym) { memberSymbol_ = sym; }

private:
    uptr<Expr> object_;
    string member_;
    Symbol *memberSymbol_ = nullptr;
};

// ===== R-values =====

class IntConst : public Rval {
private:
    int value;

public:
    IntConst(Location l, int v);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    int getValue() const { return value; }
};

class CharConst : public Rval {
private:
    unsigned char value;

public:
    CharConst(Location l, unsigned char v);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    unsigned char getValue() const { return value; }
};

class TrueConst : public Rval {
public:
    explicit TrueConst(Location l);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
};

class FalseConst : public Rval {
public:
    explicit FalseConst(Location l);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
};

// ===== Expressions =====

class LValueExpr : public Expr {
public:
    LValueExpr(Location l, uptr<Lval> val);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    Lval *lvalue() const { return value.get(); }
    uptr<Lval> releaseLVal() { return std::move(value); }

private:
    uptr<Lval> value;
};

class ParenExpr : public Expr {
public:
    ParenExpr(Location l, uptr<Expr> expr);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    Expr *innerExpr() const { return inner.get(); }

private:
    uptr<Expr> inner;
};

class FuncCall : public Expr {
public:
    FuncCall(Location l, string id, vec<uptr<Expr>> a);
    const FuncSymbol *funcSymbol() const { return symbol_; }
    void setFuncSymbol(FuncSymbol *sym) { symbol_ = sym; }
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    const string &identifier() const { return name; }
    const vec<uptr<Expr>> &arguments() const { return args; }

private:
    string name;
    vec<uptr<Expr>> args;
    // store associated symbol (callee)
    FuncSymbol *symbol_ = nullptr;
};

// Member access expression (e.g., a.b, a.func)
class MemberAccessExpr : public Expr {
public:
    MemberAccessExpr(Location l, uptr<Expr> obj, string member);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Expr *object() const { return object_.get(); }
    const string &memberName() const { return member_; }
    Symbol *memberSymbol() const { return memberSymbol_; }
    void setMemberSymbol(Symbol *sym) { memberSymbol_ = sym; }

private:
    uptr<Expr> object_;
    string member_;
    Symbol *memberSymbol_ = nullptr;
};

// Method call expression (e.g., a.func(args))
class MethodCall : public Expr {
public:
    MethodCall(Location l, uptr<Expr> obj, string method, vec<uptr<Expr>> args);
    void print(std::ostream &out) const override;
    void accept(AstVisitor &v) override;
    Expr *object() const { return object_.get(); }
    const string &methodName() const { return method_; }
    const vec<uptr<Expr>> &arguments() const { return args; }
    MethodSymbol *methodSymbol() const { return symbol_; }
    void setMethodSymbol(MethodSymbol *sym) { symbol_ = sym; }

private:
    uptr<Expr> object_;
    string method_;
    vec<uptr<Expr>> args;
    MethodSymbol *symbol_ = nullptr;
};

class UnaryExpr : public Expr {
public:
    UnaryExpr(Location l, UnOp operation, uptr<Expr> expr);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    UnOp opKind() const { return op; }
    Expr *operandExpr() const { return operand.get(); }

private:
    UnOp op;
    uptr<Expr> operand;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(Location l, BinOp operation, uptr<Expr> left, uptr<Expr> right);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    BinOp opKind() const { return op; }
    Expr *leftExpr() const { return lhs.get(); }
    Expr *rightExpr() const { return rhs.get(); }

private:
    BinOp op;
    uptr<Expr> lhs;
    uptr<Expr> rhs;
};

class ArrayExpr : public Expr {
public:
    ArrayExpr(Location loc, vec<uptr<Expr>> elems);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    const vec<uptr<Expr>> &getElements() const { return elements; }

private:
    vec<uptr<Expr>> elements;
};

// ===== Conditions =====

class Cond : public Expr {
public:
    explicit Cond(Location l);
    ~Cond() override = default;
    virtual void print(std::ostream &out) const override = 0;
};

class ExprCond : public Cond {
public:
    ExprCond(Location l, uptr<Expr> e);
    void accept(AstVisitor &v) override;
    void print(std::ostream &out) const override;
    Expr *expression() const { return expr.get(); }

private:
    uptr<Expr> expr;
};

// class ParenCond : public Cond {
// public:
//     ParenCond(Location l, uptr<Cond> c);
//     void accept(AstVisitor &v) override;
//     void print(std::ostream &out) const override;
//     Cond *conditionExpr() const { return condition.get(); }

// private:
//     uptr<Cond> condition;
// };

// class NotCond : public Cond {
// public:
//     NotCond(Location l, uptr<Cond> c);
//     void accept(AstVisitor &v) override;
//     void print(std::ostream &out) const override;
//     Cond *conditionExpr() const { return condition.get(); }

// private:
//     uptr<Cond> condition;
// };

// class BinaryCond : public Cond {
// public:
//     BinaryCond(Location l, LogicOp operation, uptr<Cond> left, uptr<Cond> right);
//     void accept(AstVisitor &v) override;
//     void print(std::ostream &out) const override;
//     LogicOp opKind() const { return op; }
//     Cond *leftCond() const { return lhs.get(); }
//     Cond *rightCond() const { return rhs.get(); }

// private:
//     LogicOp op;
//     uptr<Cond> lhs;
//     uptr<Cond> rhs;
// };

// class RelCond : public Cond {
// public:
//     RelCond(Location l, BinOp operation, uptr<Expr> left, uptr<Expr> right);
//     void accept(AstVisitor &v) override;
//     void print(std::ostream &out) const override;
//     BinOp opKind() const { return op; }
//     Expr *leftExpr() const { return lhs.get(); }
//     Expr *rightExpr() const { return rhs.get(); }

// private:
//     BinOp op;
//     uptr<Expr> lhs;
//     uptr<Expr> rhs;
// };

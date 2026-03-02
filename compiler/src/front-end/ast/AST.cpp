#include "AST.hpp"
#include "ASTVisitor.hpp"
#include "Location.hpp"
#include "Types.hpp"
// ===== Base nodes =====

ASTNode::ASTNode(Location loc) : loc(loc) {}
ASTNode::~ASTNode() = default;

Expr::Expr(Location loc) : ASTNode(loc) {
}
Expr::~Expr() = default;

Stmt::Stmt(Location loc) : ASTNode(loc) {
}
Stmt::~Stmt() = default;

Lval::Lval(Location loc) : ASTNode(loc) {
}
Lval::~Lval() = default;

Rval::Rval(Location loc) : Expr(loc) {
}

// ===== Helpers on Expr/Lval =====

SemaTypePtr Expr::type() const {
  return resolvedType_;
}

void Expr::setType(SemaTypePtr type) {
  resolvedType_ = std::move(type);
}

bool Expr::isLValue() const {
  return isLValue_;
}

void Expr::setLValue(bool v) {
  isLValue_ = v;
}

bool Expr::isAssignable() const {
  return assignable_;
}

void Expr::setAssignable(bool v) {
  assignable_ = v;
}
bool Expr::isConstExpr() const {
  return constExpr_;
}

void Expr::setConstExpr(bool v) {
  constExpr_ = v;
}

SemaTypePtr Lval::type() const {
  return resolvedType_;
}

void Lval::setType(SemaTypePtr type) {
  resolvedType_ = std::move(type);
}

bool Lval::isAssignable() const {
  return assignable_;
}

void Lval::setAssignable(bool v) {
  assignable_ = v;
}

// ===== Types =====

Type::Type(Location l, DataType::DataType b, vec<std::optional<int>> d)
    : ASTNode(l), base(b), dims(std::move(d)) {
}

void Type::accept(AstVisitor &v) {
  v.visit(*this);
}

DataType::DataType Type::data_type() const {
  return base;
}

const vec<std::optional<int>> &Type::dimensions() const {
  return dims;
}

void Type::setTypeName(const string &name) {
  type_name = name;
}

const optional<string> &Type::getName() const {
  return type_name;
}

FuncParameterType::FuncParameterType(Location l, bool ref, DataType::DataType type)
    : Type(l, type), by_ref(ref) {
}

FuncParameterType::FuncParameterType(Location l, bool ref, DataType::DataType type, vec<std::optional<int>> d)
    : Type(l, type, std::move(d)), by_ref(ref) {
}

void FuncParameterType::accept(AstVisitor &v) {
  v.visit(*this);
}

bool FuncParameterType::isByRef() const {
  return by_ref;
}

// ===== Blocks =====

Block::Block(Location l, vec<uptr<Stmt>> stmts)
    : ASTNode(l), statements(std::move(stmts)) {
}

void Block::accept(AstVisitor &v) {
  v.visit(*this);
}

// ===== Definitions =====

Decl::Decl(Location l) : ASTNode(l) {
}

Program::Program(Location l, vec<uptr<ASTNode>> ds)
    : ASTNode(l), defs(std::move(ds)) {
}

void Program::accept(AstVisitor &v) {
  v.visit(*this);
}

FuncParameterDecl::FuncParameterDecl(Location l, vec<string> names, uptr<FuncParameterType> t)
    : Decl(l), identifiers(std::move(names)), type(std::move(t)) {
}

void FuncParameterDecl::accept(AstVisitor &v) {
  v.visit(*this);
}

const vec<string> &FuncParameterDecl::names() const {
  return identifiers;
}

const FuncParameterType *FuncParameterDecl::parameterType() const {
  return type.get();
}

Header::Header(Location l, string n, optional<DataType::DataType> r, vec<uptr<FuncParameterDecl>> p)
    : Decl(l), name(std::move(n)), return_type(std::move(r)), params(std::move(p)) {
}

void Header::accept(AstVisitor &v) {
  v.visit(*this);
}

const string &Header::identifier() const {
  return name;
}

const vec<uptr<FuncParameterDecl>> &Header::parameters() const {
  return params;
}

optional<DataType::DataType> Header::returnType() const {
  return return_type;
}

VarDef::VarDef(Location l, vec<string> ids, uptr<Type> t, optional<uptr<Expr>> init)
    : Stmt(l), names(std::move(ids)), declared_type(std::move(t)), init_expr_(std::move(init)) {
}

void VarDef::accept(AstVisitor &v) {
  v.visit(*this);
}

FuncDecl::FuncDecl(Location l, uptr<Header> h)
    : Decl(l), header(std::move(h)) {
}

void FuncDecl::accept(AstVisitor &v) {
  v.visit(*this);
}

FuncDef::FuncDef(Location l, uptr<Header> h, uptr<Block> b)
    : Stmt(l), header(std::move(h)), body(std::move(b)) {
}

void FuncDef::accept(AstVisitor &v) {
  v.visit(*this);
}

bool FuncDef::isEntrypoint() {
  return isEntrypoint_;
}

bool FuncDef::isMethod() {
  return isMethod_;
}

void FuncDef::setIsMethod(bool cond) {
  isMethod_ = cond;
}

void FuncDef::setEntrypoint(bool cond) {
  isEntrypoint_ = cond;
}

ClassDecl::ClassDecl(Location l, string n, vec<uptr<VarDef>> f, vec<uptr<FuncDef>> m)
    : Decl(l), name(std::move(n)), fields(std::move(f)), methods(std::move(m)) {
}
void ClassDecl::accept(AstVisitor &v) {
  v.visit(*this);
}


// ===== Statements =====

SkipStmt::SkipStmt(Location l) : Stmt(l) {
}
void SkipStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

ExitStmt::ExitStmt(Location l) : Stmt(l) {
}
void ExitStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

AssignStmt::AssignStmt(Location l, uptr<Lval> left, uptr<Expr> right)
    : Stmt(l), lhs(std::move(left)), rhs(std::move(right)) {
}
void AssignStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

ReturnStmt::ReturnStmt(Location l, uptr<Expr> expr)
    : Stmt(l), value(std::move(expr)) {
}
void ReturnStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

// ProcCall
ProcCall::ProcCall(Location l, string id, vec<uptr<Expr>> a) : Stmt(l), name(std::move(id)), args(std::move(a)) {}

void ProcCall::accept(AstVisitor &v) {
  v.visit(*this);
}

BreakStmt::BreakStmt(Location l, optional<string> lbl)
    : Stmt(l), label(std::move(lbl)) {
}
void BreakStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

ContinueStmt::ContinueStmt(Location l, optional<string> lbl)
    : Stmt(l), label(std::move(lbl)) {
}
void ContinueStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

IfStmt::IfStmt(Location l, uptr<Cond> cond, uptr<Block> then_block, vec<std::pair<uptr<Cond>, uptr<Block>>> elifs, std::optional<uptr<Block>> else_block)
    : Stmt(l),
      condition(std::move(cond)),
      then_branch(std::move(then_block)),
      elif_branches(std::move(elifs)),
      else_branch(std::move(else_block)) {
}
void IfStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

LoopStmt::LoopStmt(Location l, uptr<Cond> cond, uptr<Block> blk)
    : Stmt(l), condition(std::move(cond)), body(std::move(blk)) {
}
void LoopStmt::accept(AstVisitor &v) {
  v.visit(*this);
}

// ===== L-values =====

IdLVal::IdLVal(Location l, string id)
    : Lval(l), name(std::move(id)) {}
void IdLVal::accept(AstVisitor &v) {
  v.visit(*this);
}

StringLiteralLVal::StringLiteralLVal(Location l, string v)
    : Lval(l), value(std::move(v)) {}
void StringLiteralLVal::accept(AstVisitor &v) {
  v.visit(*this);
}

IndexLVal::IndexLVal(Location l, uptr<Lval> b, uptr<Expr> idx)
    : Lval(l), base(std::move(b)), index(std::move(idx)) {}
void IndexLVal::accept(AstVisitor &v) {
  v.visit(*this);
}

MemberAccessLVal::MemberAccessLVal(Location l, uptr<Expr> obj, string member)
    : Lval(l), object_(std::move(obj)), member_(member) {}
void MemberAccessLVal::accept(AstVisitor &v) {
  v.visit(*this);
}

// ===== R-values / expressions =====

IntConst::IntConst(Location l, int v)
    : Rval(l), value(v) {}
void IntConst::accept(AstVisitor &v) {
  v.visit(*this);
}

CharConst::CharConst(Location l, unsigned char v)
    : Rval(l), value(v) {}
void CharConst::accept(AstVisitor &v) {
  v.visit(*this);
}

TrueConst::TrueConst(Location l)
    : Rval(l) {}
void TrueConst::accept(AstVisitor &v) {
  v.visit(*this);
}

FalseConst::FalseConst(Location l)
    : Rval(l) {}
void FalseConst::accept(AstVisitor &v) {
  v.visit(*this);
}

LValueExpr::LValueExpr(Location l, uptr<Lval> val)
    : Expr(l), value(std::move(val)) {}
void LValueExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

ParenExpr::ParenExpr(Location l, uptr<Expr> expr)
    : Expr(l), inner(std::move(expr)) {}
void ParenExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

FuncCall::FuncCall(Location l, string id, vec<uptr<Expr>> a)
    : Expr(l), name(std::move(id)), args(std::move(a)) {}
void FuncCall::accept(AstVisitor &v) {
  v.visit(*this);
}

MemberAccessExpr::MemberAccessExpr(Location l, uptr<Expr> obj, string member)
    : Expr(l), object_(std::move(obj)), member_(member) {}
void MemberAccessExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

MethodCall::MethodCall(Location l, uptr<Expr> obj, string method, vec<uptr<Expr>> args)
    : Expr(l), object_(std::move(obj)), method_(method), args(std::move(args)) {}
void MethodCall::accept(AstVisitor &v) {
  v.visit(*this);
}

NewExpr::NewExpr(Location l, string clsName, vec<uptr<Expr>> args)
    : Expr(l), clsName(std::move(clsName)), args(std::move(args)) {}
void NewExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

UnaryExpr::UnaryExpr(Location l, UnOp operation, uptr<Expr> expr)
    : Expr(l), op(operation), operand(std::move(expr)) {}
void UnaryExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

BinaryExpr::BinaryExpr(Location l, BinOp operation, uptr<Expr> left, uptr<Expr> right)
    : Expr(l), op(operation), lhs(std::move(left)), rhs(std::move(right)) {}
void BinaryExpr::accept(AstVisitor &v) {
  v.visit(*this);
}


ArrayExpr::ArrayExpr(Location loc, vec<uptr<Expr>> elems) : Expr(loc), elements(std::move(elems)) {}
void ArrayExpr::accept(AstVisitor &v) {
  v.visit(*this);
}

// SelfExpr::SelfExpr(Location loc) : Expr(loc) {}
// void SelfExpr::accept(AstVisitor &v) {
//     v.visit(*this);
// }
//
// SuperExpr::SuperExpr(Location loc) : Expr(loc) {}
// void SuperExpr::accept(AstVisitor &v) {
//     v.visit(*this);
// }
// ===== Conditions =====

Cond::Cond(Location l)
    : Expr(l) {}

ExprCond::ExprCond(Location l, uptr<Expr> e)
    : Cond(l), expr(std::move(e)) {}
void ExprCond::accept(AstVisitor &v) {
  v.visit(*this);
}

// ParenCond::ParenCond(Location l, uptr<Cond> c)
//     : Cond(l), condition(std::move(c)) {}
// void ParenCond::accept(AstVisitor &v) {
//     v.visit(*this);
// }

// NotCond::NotCond(Location l, uptr<Cond> c)
//     : Cond(l), condition(std::move(c)) {}
// void NotCond::accept(AstVisitor &v) {
//     v.visit(*this);
// }

// BinaryCond::BinaryCond(Location l, LogicOp operation, uptr<Cond> left, uptr<Cond> right)
//     : Cond(l), op(operation), lhs(std::move(left)), rhs(std::move(right)) {}
// void BinaryCond::accept(AstVisitor &v) {
//     v.visit(*this);
// }

// RelCond::RelCond(Location l, BinOp operation, uptr<Expr> left, uptr<Expr> right)
//     : Cond(l), op(operation), lhs(std::move(left)), rhs(std::move(right)) {}
// void RelCond::accept(AstVisitor &v) {
//     v.visit(*this);
// }

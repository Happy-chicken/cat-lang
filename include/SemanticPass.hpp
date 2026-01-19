#pragma once
#include "AST.hpp"
#include "ASTVisitor.hpp"
#include "Location.hpp"
#include "SemaType.hpp"
#include "SemanticCtx.hpp"
#include "Types.hpp"
#include <memory>

class SemanticCtx;

struct ParamInfo {
    std::string name;
    Location loc;
    SemaTypePtr type;
    Symbol::ParamPass passMode = Symbol::ParamPass::BY_VAL;
};

class SemanticPass : public AstVisitor {
public:
    SemanticPass(SemanticCtx &ctx) : semanticCtx(ctx) {}

    void visit(Type &node) override;
    void visit(FuncParameterType &node) override;

    void visit(Program &node) override;
    void visit(Header &node) override;
    void visit(ClassDef &node) override;
    void visit(VarDef &node) override;
    void visit(FuncDecl &node) override;
    void visit(FuncDef &node) override;
    void visit(FuncParameterDef &node) override;

    void visit(Block &node) override;
    void visit(SkipStmt &node) override;
    void visit(ExitStmt &node) override;
    void visit(AssignStmt &node) override;
    void visit(ReturnStmt &node) override;
    void visit(ProcCall &node) override;
    void visit(BreakStmt &node) override;
    void visit(ContinueStmt &node) override;
    void visit(IfStmt &node) override;
    void visit(LoopStmt &node) override;

    void visit(IdLVal &node) override;
    void visit(StringLiteralLVal &node) override;
    void visit(IndexLVal &node) override;
    void visit(MemberAccessLVal &node) override;

    void visit(IntConst &node) override;
    void visit(CharConst &node) override;
    void visit(TrueConst &node) override;
    void visit(FalseConst &node) override;

    void visit(LValueExpr &node) override;
    void visit(ParenExpr &node) override;
    void visit(FuncCall &node) override;
    void visit(MemberAccessExpr &node) override;
    void visit(MethodCall &node) override;
    void visit(UnaryExpr &node) override;
    void visit(BinaryExpr &node) override;
    void visit(ExprCond &node) override;


private:
    static bool isIntType(const SemaTypePtr &t) {
        return t && t->getKind() == SemaType::TypeKind::INT;
    }
    static bool isChartype(const SemaTypePtr &t) {
        return t && t->getKind() == SemaType::TypeKind::CHAR;
    }
    static bool isBoolType(const SemaTypePtr &t) {
        return t && t->getKind() == SemaType::TypeKind::BOOL;
    }
    static bool isByteType(const SemaTypePtr &t) {
        return t && t->getKind() == SemaType::TypeKind::BYTE;
    }
    static bool isArrayType(const SemaTypePtr &t) {
        return t && t->getKind() == SemaType::TypeKind::ARRAY;
    }
    static bool isCharStrCompatible(const SemaTypePtr &a, const SemaTypePtr &b) {
        if (!a || !b) {
            return false;
        }
        if (a->getKind() != SemaType::TypeKind::STR) {
            return false;
        }
        auto arr_type = std::dynamic_pointer_cast<const ArrayType>(b);
        if (!arr_type) {
            return false;
        }
        auto elem_type = arr_type->elementType();
        if (elem_type->getKind() == SemaType::TypeKind::ARRAY) {
            return false;
        }
        return elem_type->getKind() == SemaType::TypeKind::CHAR;
    }
    static bool typesEqual(const SemaTypePtr &a, const SemaTypePtr &b) {
        if (a == b) return true;
        if (isCharStrCompatible(a, b) || isCharStrCompatible(b, a)) return true;
        if (!a || !b) return false;
        return a->equals(*b);
    }
    static bool arrayTypesCompatible(const ArrayType *actual, const ArrayType *expected);
    static bool typesCompatible(const SemaTypePtr &actual, const SemaTypePtr &expected);
    static SemaTypePtr scalarType(DataType::DataType dt);
    bool validateDimension(const std::optional<int> &dim, bool allowUnsized, const Location &loc);
    SemaTypePtr buildArrayType(const Location &loc, SemaTypePtr base, const vec<std::optional<int>> &dims, bool allowUnsizedFirst);
    SemaTypePtr resolveType(const Type &node, bool allowUnsizedFirst = false);
    SemaTypePtr resolveParamType(const FuncParameterType &node, Symbol::ParamPass &pass);
    static std::string typeToString(const SemaTypePtr &type);

    // Semantic analysis helpers
    bool collectParams(const Header &header, std::vector<ParamInfo> &params);
    bool signaturesMatch(bool isProcedure, const SemaTypePtr &returnType, const std::vector<ParamInfo> &params, const Symbol *symbol);
    bool checkArguments(const vec<uptr<Expr>> &args, const std::vector<ParamSymbol *> &params, const std::string &callee, const Location &loc);

    //
    void VerifyEntryPoint(const vec<uptr<ASTNode>> &defs);

private:
    SemanticCtx &semanticCtx;
};
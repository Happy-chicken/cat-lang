#include "SemanticPass.hpp"
#include "AST.hpp"
#include "Diagnostics.hpp"
#include "SemaType.hpp"
#include "SemanticCtx.hpp"
#include "Symbol.hpp"
#include "Types.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

void SemanticPass::visit(Type &node) {
    resolveType(node);
}
void SemanticPass::visit(FuncParameterType &node) {
    // pass by value by default
    auto pass = Symbol::ParamPass::BY_VAL;
    resolveParamType(node, pass);
}

void SemanticPass::visit(Program &node) {
    // program entry point
    auto &defs = node.getDefs();
    FuncDef *main_fun = nullptr;
    // first pass: collect function declareation and class definitions
    for (const auto &d: defs) {
        if (auto *funcDef = dynamic_cast<FuncDef *>(d.get())) {
            // declareFunctionHeader(funcDef->funcHeader());
        } else if (auto *classDef = dynamic_cast<ClassDef *>(d.get())) {
            // declareClassDefinition(classDef);
        } else if (auto *funcDecl = dynamic_cast<FuncDecl *>(d.get())) {
            // declareFunctionHeader(funcDecl->funcHeader());
        }
    }
    // second pass: visit all definitions
}
void SemanticPass::visit(Header &node) { std::cout << "Header\n"; }
void SemanticPass::visit(ClassDef &node) { std::cout << "ClassDef\n"; }
void SemanticPass::visit(FuncDecl &n) {
    // nothing to do
}
void SemanticPass::visit(FuncDef &node) {
    // nothing to do
}
void SemanticPass::visit(VarDef &node) {
    // nothing to do
}
void SemanticPass::visit(FuncParameterDef &node) {
    // nothing to do
}
void SemanticPass::visit(Block &node) {
    std::cout << "Block\n";
}

void SemanticPass::visit(SkipStmt &node) { std::cout << "SkipStmt\n"; }
void SemanticPass::visit(ExitStmt &node) { std::cout << "ExitStmt\n"; }
void SemanticPass::visit(IfStmt &node) { std::cout << "IfStmt\n"; }
void SemanticPass::visit(LoopStmt &node) { std::cout << "LoopStmt\n"; }
void SemanticPass::visit(ReturnStmt &node) { std::cout << "ReturnStmt\n"; }
void SemanticPass::visit(AssignStmt &node) { std::cout << "AssignStmt\n"; }
void SemanticPass::visit(BreakStmt &node) { std::cout << "BreakStmt\n"; }
void SemanticPass::visit(ContinueStmt &node) { std::cout << "ContinueStmt\n"; }
void SemanticPass::visit(ProcCall &node) { std::cout << "ProcCall\n"; }

void SemanticPass::visit(BinaryExpr &node) { std::cout << "BinaryExpr\n"; }
void SemanticPass::visit(UnaryExpr &node) { std::cout << "UnaryExpr\n"; }
void SemanticPass::visit(LValueExpr &node) { std::cout << "LValueExpr\n"; }
void SemanticPass::visit(IdLVal &node) { std::cout << "IdLVal\n"; }
void SemanticPass::visit(IndexLVal &node) { std::cout << "IndexLVal\n"; }
void SemanticPass::visit(StringLiteralLVal &node) { std::cout << "StringLiteralLVal\n"; }
void SemanticPass::visit(ParenExpr &node) { std::cout << "ParenExpr\n"; }
void SemanticPass::visit(FuncCall &node) { std::cout << "FuncCall\n"; }

void SemanticPass::visit(IntConst &node) { std::cout << "IntConst\n"; }
void SemanticPass::visit(CharConst &node) { std::cout << "CharConst\n"; }
void SemanticPass::visit(TrueConst &node) { std::cout << "TrueConst\n"; }
void SemanticPass::visit(FalseConst &node) { std::cout << "FalseConst\n"; }

void SemanticPass::visit(ExprCond &node) { std::cout << "ExprCond\n"; }


//
void SemanticPass::setMainFunction(FuncDef *mainFunc) {
    if (!mainFunc) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "No 'main' function defined.");
    }
    mainFunc->setEntrypoint(true);
    mainFunc->accept(*this);
    auto *header = mainFunc->funcHeader();
    if (!header) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function has no header.");
    }
    FuncSymbol *mainSymbol = header->symbol();
    if (!mainSymbol) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function has no associated symbol.");
    }
    if (!mainSymbol->isProcedure()) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function must be a procedure with no return type.");
    }
    if (!mainSymbol->getParams().empty()) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function must not have parameters.");
    }
}

// Type resolution helper
bool SemanticPass::arrayTypesCompatible(const ArrayType *actual, const ArrayType *expected) { return true; }
bool SemanticPass::typesCompatible(const SemaTypePtr &actual, const SemaTypePtr &expected) { return false; }

bool SemanticPass::validateDimension(const std::optional<int> &dim, bool allowUnsized, const Location &loc) {
    if (!dim.has_value()) {
        if (allowUnsized) {
            return true;
        } else {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, loc, "Array dimension must be specified.");
            return false;
        }
    }
    if (*dim <= 0) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, loc, "Array dimension must be a positive integer.");
        return false;
    }
    return true;
}
SemaTypePtr SemanticPass::buildArrayType(const Location &loc, SemaTypePtr base, const vec<std::optional<int>> &dims, bool allowUnsizedFirst) {
    SemaTypePtr currentType = std::move(base);
    for (std::size_t i = dims.size(); i-- > 0;) {
        const auto &dim = dims[i];
        if (!validateDimension(dim, allowUnsizedFirst && i == 0, loc)) {
            return nullptr;
        }
        std::optional<std::size_t> extent;
        if (dim.has_value()) {
            extent = static_cast<std::size_t>(*dim);
        }
        currentType = makeArrayType(currentType, extent);
    }
    return currentType;
}
SemaTypePtr SemanticPass::scalarType(DataType::DataType dt) {
    switch (dt) {
        case DataType::DataType::INT:
            return makeIntType();
        // case DataType::DataType::CHAR:
        //     return makeCharType();
        // case DataType::DataType::BOOL:
        //     return makeBoolType();
        case DataType::DataType::BYTE:
            return makeByteType();
        default:
            return nullptr;
    }
}
SemaTypePtr SemanticPass::resolveType(const Type &node, bool allowUnsizedFirst) {
    auto base_type = scalarType(node.data_type());
    return buildArrayType(node.loc, base_type, node.dimensions(), allowUnsizedFirst);
}
SemaTypePtr SemanticPass::resolveParamType(const FuncParameterType &node, Symbol::ParamPass &pass) {
    const bool isArray = !node.dimensions().empty();
    auto resolvedType = resolveType(node, true);
    if (!resolvedType) {
        return nullptr;
    }
    // 数组默认按引用传递
    if (isArray) {
        pass = Symbol::ParamPass::BY_REF;
    } else {
        pass = node.isByRef() ? Symbol::ParamPass::BY_REF : Symbol::ParamPass::BY_VAL;
    }
    return resolvedType;
}
std::string SemanticPass::typeToString(const SemaTypePtr &type) { return "ok"; }

// Semantic analysis helpers
bool SemanticPass::collectParams(const Header &header, std::vector<ParamInfo> &params) {
    std::unordered_set<string> seen;
    const string &name = header.identifier();
    for (const auto &param: header.parameters()) {
        if (!param) {
            continue;
        }
        auto parm_type = param->parameterType();
        if (!parm_type) {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, param->loc, "Parameter in function '" + name + "' has no type.");
            continue;
        }
        Symbol::ParamPass pass;
        auto resolvedType = resolveParamType(*parm_type, pass);
        if (!resolvedType) {
            continue;
        }

        for (const auto &param_name: param->names()) {
            if (seen.insert(param_name).second == false) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, param->loc, "Duplicate parameter name '" + param_name + "' in function '" + name + "'.");
                continue;
            }
            ParamInfo pinfo;
            pinfo.name = param_name;
            pinfo.type = resolvedType;
            pinfo.loc = param->loc;
            pinfo.passMode = pass;
            params.push_back(std::move(pinfo));
        }
    }
    return true;
}
bool SemanticPass::signaturesMatch(bool isProcedure, const SemaTypePtr &returnType, const std::vector<ParamInfo> &params, const Symbol *symbol) { return true; }
bool SemanticPass::checkArguments(const vec<uptr<Expr>> &args, const std::vector<ParamSymbol *> &params, const std::string &callee, const Location &loc) { return true; }
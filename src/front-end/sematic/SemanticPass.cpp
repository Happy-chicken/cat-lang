#include "SemanticPass.hpp"
#include "AST.hpp"
#include "Diagnostics.hpp"
#include "Operator.hpp"
#include "SemaType.hpp"
#include "SemanticCtx.hpp"
#include "Symbol.hpp"
#include "Types.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
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
    // first pass: collect function declareation and class definitions
    for (const auto &d: defs) {
        if (auto *funcDef = dynamic_cast<FuncDef *>(d.get())) {
            declareFunctionHeader(funcDef->funcHeader());
        } else if (auto *classDef = dynamic_cast<ClassDef *>(d.get())) {
            // declareClassDefinition(classDef);
        } else if (auto *funcDecl = dynamic_cast<FuncDecl *>(d.get())) {
            declareFunctionHeader(funcDecl->funcHeader());
        }
    }
    // second pass: visit all definitions
    for (const auto &d: defs) {
        d->accept(*this);
    }
    // locate main function
    VerifyEntryPoint(defs);
}
void SemanticPass::visit(Header &node) { std::cout << "Header\n"; }
void SemanticPass::visit(ClassDef &node) { std::cout << "ClassDef\n"; }
void SemanticPass::visit(FuncDecl &node) {
    auto *header = node.funcHeader();
    if (!header) {
        return;
    }

    // Collect header info directly from AST
    const std::string &name = header->identifier();
    const auto returnTypeOpt = header->returnType();
    const bool isProcedure = !returnTypeOpt.has_value();
    const SemaTypePtr returnType = isProcedure ? makeVoidType() : scalarType(*returnTypeOpt);

    // Collect parameters
    std::vector<ParamInfo> params;
    if (!collectParams(*header, params)) {
        return;// errors already reported
    }

    // Check for existing declaration
    auto existing = semanticCtx.lookupLocalSymbol(name);
    Symbol *symbol = existing.symbol;
    if (symbol) {
        if (!signaturesMatch(isProcedure, returnType, params, symbol)) {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "forward declaration of '" + name + "' conflicts with previous declaration");
            throw std::runtime_error("semantic analysis failed");
        }
        if (symbol->isDefined()) {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "symbol '" + name + "' already defined");
            throw std::runtime_error("semantic analysis failed");
        }
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Info, Diagnostics::Phase::SemanticAnalysis, symbol->getLocation(), "previous declaration here");
        header->setSymbol(static_cast<FuncSymbol *>(symbol));
        throw std::runtime_error("semantic analysis failed");
    }

    // Create function signature and symbol
    std::vector<SemaTypePtr> paramTypes;
    paramTypes.reserve(params.size());
    for (const auto &p: params) {
        paramTypes.push_back(p.type);
    }
    auto sig = makeFuncType(returnType, std::move(paramTypes));
    auto func = std::make_unique<FuncSymbol>(name, std::move(sig), isProcedure, header->loc);

    // Create temporary scope for parameters (orphaned after FuncDecl)
    semanticCtx.beginScope();
    for (const auto &paramInfo: params) {
        auto p = std::make_unique<ParamSymbol>(paramInfo.name, paramInfo.type, paramInfo.passMode, paramInfo.loc);
        p->setDefiningFunc(func.get());
        auto result = semanticCtx.declareSymbol(std::move(p));
        if (result.symbol) {
            func->addParam(static_cast<ParamSymbol *>(result.symbol));
        }
    }
    semanticCtx.endScope();

    FuncSymbol *raw = func.get();
    if (auto frame = semanticCtx.currentFunction()) {
        raw->setDefiningFunc(frame->symbol);
    }
    semanticCtx.declareSymbol(std::move(func));
    raw->markForwardDeclaration();
    header->setSymbol(raw);
}
void SemanticPass::visit(FuncDef &node) {
    auto *header = node.funcHeader();
    if (!header) {
        return;
    }

    // Collect header info directly from AST
    const std::string &name = header->identifier();
    const auto returnTypeOpt = header->returnType();
    const bool isProcedure = !returnTypeOpt.has_value();
    const SemaTypePtr returnType = isProcedure ? makeVoidType() : scalarType(*returnTypeOpt);

    // Collect parameters
    std::vector<ParamInfo> params;
    if (!collectParams(*header, params)) {
        return;// errors already reported
    }

    // Check for existing declaration
    auto existing = semanticCtx.lookupLocalSymbol(name);
    Symbol *symbol = existing.symbol;
    bool wasForwardDeclared = false;

    if (symbol) {
        if (!signaturesMatch(isProcedure, returnType, params, symbol)) {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "definition of '" + name + "' does not match prior declaration");
            throw std::runtime_error("semantic analysis failed");
        }
        if (symbol->isDefined()) {
            semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "redefinition of '" + name + "'");
            throw std::runtime_error("semantic analysis failed");
        }
        wasForwardDeclared = true;
    }

    FuncSymbol *fsym = nullptr;

    if (wasForwardDeclared) {
        // Use existing symbol from forward declaration
        fsym = static_cast<FuncSymbol *>(symbol);
        // Clear old params - they were orphaned when the FuncDecl's scope closed
        fsym->clearParams();
    } else {
        // Create new function symbol
        std::vector<SemaTypePtr> paramTypes;
        paramTypes.reserve(params.size());
        for (const auto &p: params) {
            paramTypes.push_back(p.type);
        }
        auto sig = makeFuncType(returnType, std::move(paramTypes));
        auto func = std::make_unique<FuncSymbol>(name, std::move(sig), isProcedure, header->loc);
        fsym = func.get();
        semanticCtx.declareSymbol(std::move(func));
    }

    fsym->setDefiningFunc(semanticCtx.currentFunction() ? semanticCtx.currentFunction()->symbol : nullptr);
    header->setSymbol(fsym);

    semanticCtx.beginScope();

    // Create function frame
    sptr<SemanticCtx::FunctionFrame> frame = std::make_shared<SemanticCtx::FunctionFrame>();
    frame->symbol = fsym;
    frame->is_procedure = isProcedure;
    frame->return_type = returnType;
    semanticCtx.enterFunction(frame);

    // Declare parameters in the new scope
    for (const auto &paramInfo: params) {
        auto sym = std::make_unique<ParamSymbol>(paramInfo.name, paramInfo.type, paramInfo.passMode, paramInfo.loc);
        sym->setDefiningFunc(fsym);
        auto result = semanticCtx.declareSymbol(std::move(sym));
        if (result.symbol) {
            fsym->addParam(static_cast<ParamSymbol *>(result.symbol));
        }
    }

    for (auto &def: node.localDefs()) {
        if (def) {
            def->accept(*this);
        }
    }

    if (auto *body = node.funcBody()) {
        body->accept(*this);
    }

    semanticCtx.leaveFunction();
    semanticCtx.endScope();
    fsym->markDefined();
}
void SemanticPass::visit(VarDef &node) {
    node.symbols().clear();
    Type *typeNode = node.declaredType();
    if (!typeNode) {
        return;
    }
    auto resolved_type = resolveType(*typeNode);
    if (!resolved_type) {
        return;
    }
    for (const auto &id: node.identifiers()) {
        auto sym = std::make_unique<VarSymbol>(id, resolved_type, node.loc);
        VarSymbol *raw = sym.get();
        if (auto frame = semanticCtx.currentFunction()) {
            raw->setDefiningFunc(static_cast<FuncSymbol *>(frame->symbol));
        }
        semanticCtx.declareSymbol(std::move(sym));
        node.symbols().push_back(raw);
    }
}
void SemanticPass::visit(FuncParameterDef &node) {
    if (auto *t = node.parameterType()) {
        t->accept(*this);
    }
}
void SemanticPass::visit(Block &node) {
    for (auto &stmt: node.statementsList()) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
}

void SemanticPass::visit(SkipStmt &node) { std::cout << "SkipStmt\n"; }
void SemanticPass::visit(ExitStmt &node) { std::cout << "ExitStmt\n"; }
void SemanticPass::visit(IfStmt &node) { std::cout << "IfStmt\n"; }
void SemanticPass::visit(LoopStmt &node) { std::cout << "LoopStmt\n"; }
void SemanticPass::visit(ReturnStmt &node) { std::cout << "ReturnStmt\n"; }
void SemanticPass::visit(AssignStmt &node) {
    auto *lhs = node.left();
    auto *rhs = node.right();
    if (lhs) {
        lhs->accept(*this);
    }
    if (rhs) {
        rhs->accept(*this);
    }
    auto leftType = lhs ? lhs->type() : SemaTypePtr{};
    auto rightType = rhs ? rhs->type() : SemaTypePtr{};
    if (!leftType) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "invalid assignment target");
        throw std::runtime_error("semantic analysis failed");
    }
    if (isArrayType(leftType)) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "cannot assign to an array value");
        throw std::runtime_error("semantic analysis failed");
    }
    if (lhs && !lhs->isAssignable()) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "left-hand side of assignment is not assignable");
        throw std::runtime_error("semantic analysis failed");
    }
    if (!typesEqual(leftType, rightType)) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "assignment type mismatch: left is '" + typeToString(leftType) + "', right is '" + typeToString(rightType) + "'");
        throw std::runtime_error("semantic analysis failed");
    }
}
void SemanticPass::visit(BreakStmt &node) { std::cout << "BreakStmt\n"; }
void SemanticPass::visit(ContinueStmt &node) { std::cout << "ContinueStmt\n"; }
void SemanticPass::visit(ProcCall &node) { std::cout << "ProcCall\n"; }

void SemanticPass::visit(BinaryExpr &node) {
    auto *lhs = node.leftExpr();
    auto *rhs = node.rightExpr();
    if (lhs) {
        lhs->accept(*this);
    }
    if (rhs) {
        rhs->accept(*this);
    }
    auto leftType = lhs ? lhs->type() : SemaTypePtr{};
    auto rightType = rhs ? rhs->type() : SemaTypePtr{};
    switch (node.opKind()) {
        case BinOp::Add:
        case BinOp::Sub:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Mod:
            if (!typesEqual(leftType, rightType) ||
                !(isIntType(leftType) || isByteType(leftType))) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "arithmetic operands must both be int or byte");
            }
            node.setType(leftType);
            break;
        case BinOp::AndBits:
        case BinOp::OrBits:
            if (!isByteType(leftType) || !typesEqual(leftType, rightType)) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "'&' and '|' require byte operands");
            }
            node.setType(makeByteType());
            break;
        case BinOp::And:
        case BinOp::Or:
            if (!isBoolType(leftType) || !typesEqual(leftType, rightType)) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "'and' and 'or' require bool operands");
            }
            node.setType(makeBoolType());
            break;
        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::Gt:
        case BinOp::Lt:
        case BinOp::Ge:
        case BinOp::Le:
            if (!typesEqual(leftType, rightType) ||
                !(isIntType(leftType) || isByteType(leftType))) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "comparison operands must both be int or byte");
            }
            node.setType(makeBoolType());
            break;
    }
    node.setLValue(false);
    node.setAssignable(false);
}
void SemanticPass::visit(UnaryExpr &node) {
    auto *operand = node.operandExpr();
    if (operand) {
        operand->accept(*this);
    }
    auto operandType = operand ? operand->type() : SemaTypePtr{};
    switch (node.opKind()) {
        case UnOp::Plus:
        case UnOp::Minus:
            if (!isIntType(operandType)) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "unary '+' and '-' require int operand");
            }
            node.setType(makeIntType());
            break;
        case UnOp::Not:
            if (!isBoolType(operandType)) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "'!' requires byte operand");
            }
            node.setType(makeBoolType());
            break;
    }
    node.setLValue(false);
    node.setAssignable(false);
}
void SemanticPass::visit(LValueExpr &node) {
    auto *value = node.lvalue();
    if (value) {
        value->accept(*this);
    }
    node.setType(value ? value->type() : SemaTypePtr{});
    node.setLValue(true);
    node.setAssignable(value ? value->isAssignable() : false);
}
void SemanticPass::visit(IdLVal &node) {
    LookupResult lookup = semanticCtx.lookup(node.identifier());
    Symbol *symbol = lookup.symbol;
    if (!lookup.found() || !(symbol->isVariable() || symbol->isParameter())) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "unknown variable '" + node.identifier() + "'");
        node.setType(nullptr);
        node.setAssignable(false);
        node.setSymbol(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }
    node.setSymbol(symbol);
    node.setType(symbol->getType());
    node.setAssignable(true);
}
void SemanticPass::visit(IndexLVal &node) { std::cout << "IndexLVal\n"; }
void SemanticPass::visit(StringLiteralLVal &node) {
    // string is equivalent to array of chars in this Sematic analysis
    // str == char[]
    auto len = static_cast<std::size_t>(node.literal().size() + 1);
    node.setType(makeStrType(len - 2));// exclude quotes
    node.setAssignable(false);
}
void SemanticPass::visit(ParenExpr &node) {
    auto *inner = node.innerExpr();
    if (inner) {
        inner->accept(*this);
    }
    node.setType(inner ? inner->type() : SemaTypePtr{});
    node.setLValue(inner && inner->isLValue());
    node.setAssignable(inner && inner->isAssignable());
}
void SemanticPass::visit(FuncCall &node) { std::cout << "FuncCall\n"; }

void SemanticPass::visit(IntConst &node) {
    node.setType(makeIntType());
    node.setConstExpr(true);
}
void SemanticPass::visit(CharConst &node) {
    node.setType(makeCharType());
    node.setConstExpr(true);
}
void SemanticPass::visit(TrueConst &node) {
    node.setType(makeBoolType());
    node.setConstExpr(true);
}
void SemanticPass::visit(FalseConst &node) {
    node.setType(makeBoolType());
    node.setConstExpr(true);
}

void SemanticPass::visit(ExprCond &node) {
    if (auto *expr = node.expression()) {
        expr->accept(*this);
    }
    // Dana does not allow bare expressions as conditions - except boolean literals;
    // conditions must be relational (=, <>, <, >, <=, >=) or logical (and, or, not)
    // for variables of type byte.
    if (dynamic_cast<TrueConst *>(node.expression()) ||
        dynamic_cast<FalseConst *>(node.expression())) {
        node.setType(makeByteType());
        return;
    }
    // semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "bare expression cannot be used as condition");
    node.setType(makeByteType());
}


//

void SemanticPass::declareFunctionHeader(Header *header) {}
void SemanticPass::VerifyEntryPoint(const vec<uptr<ASTNode>> &defs) {
    FuncDef *mainFunc = nullptr;
    for (const auto &d: defs) {
        if (auto *funcDef = dynamic_cast<FuncDef *>(d.get())) {
            auto *header = funcDef->funcHeader();
            if (header && header->identifier() == "main") {
                mainFunc = funcDef;
                break;
            }
        }
    }
    if (!mainFunc) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "No 'main' function defined.");
        throw std::runtime_error("semantic analysis failed");
    }
    mainFunc->setEntrypoint(true);
    auto *header = mainFunc->funcHeader();
    if (!header) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function has no header.");
        throw std::runtime_error("semantic analysis failed");
    }
    FuncSymbol *mainSymbol = header->symbol();
    if (!mainSymbol) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function has no associated symbol.");
        throw std::runtime_error("semantic analysis failed");
    }
    // if (!mainSymbol->isProcedure()) {
    //     semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function must be a procedure with no return type.");
    // }
    // if (!mainSymbol->getParams().empty()) {
    //     semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, mainFunc->loc, "'main' function must not have parameters.");
    // }
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
            // return false;
            throw std::runtime_error("semantic analysis failed");
        }
    }
    if (*dim <= 0) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, loc, "Array dimension must be a positive integer.");
        // return false;
        throw std::runtime_error("semantic analysis failed");
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
        case DataType::DataType::STRING:
            return makeStrType();
        case DataType::DataType::BOOL:
            return makeBoolType();
        case DataType::DataType::CHAR:
            return makeCharType();
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
std::string SemanticPass::typeToString(const SemaTypePtr &type) {
    if (!type) {
        return "<invalid type>";
    }
    switch (type->getKind()) {
        case SemaType::TypeKind::INT:
            return "int";
        case SemaType::TypeKind::BOOL:
            return "bool";
        case SemaType::TypeKind::CHAR:
            return "char";
        case SemaType::TypeKind::BYTE:
            return "byte";
        case SemaType::TypeKind::STR:
            return "string";
        case SemaType::TypeKind::ARRAY: {
            const ArrayType *arrType = static_cast<const ArrayType *>(type.get());
            std::ostringstream oss;
            oss << typeToString(arrType->elementType()) << '[';
            if (arrType->size()) {
                oss << *arrType->size();
            }
            oss << ']';
            return oss.str();
        }
        case SemaType::TypeKind::FUNC:
            return "fn";
        case SemaType::TypeKind::VOID:
            return "void";
        default:
            return "<unknown type>";
    }
}

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
            throw std::runtime_error("semantic analysis failed");
        }
        Symbol::ParamPass pass;
        auto resolvedType = resolveParamType(*parm_type, pass);
        if (!resolvedType) {
            continue;
        }

        for (const auto &param_name: param->names()) {
            if (seen.insert(param_name).second == false) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, param->loc, "Duplicate parameter name '" + param_name + "' in function '" + name + "'.");
                throw std::runtime_error("semantic analysis failed");
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
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
    //  pass 1: visit all definitions
    for (const auto &d: defs) {
        d->accept(*this);
    }
    // locate main function
    VerifyEntryPoint(defs);
}
void SemanticPass::visit(Header &node) {}
void SemanticPass::visit(ClassDef &node) {
    auto cls_name = node.identifier();
    const auto &fields = node.fieldList();
    const auto &methods = node.methodList();
    const SemaTypePtr class_type = makeClassType(cls_name);

    LookupResult existing = semanticCtx.lookup(cls_name);
    Symbol *symbol = existing.symbol;
    if (existing.found()) {
        if (symbol->isDefined()) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "class '" + cls_name + "' already defined"
            );
            throw std::runtime_error("semantic analysis failed");
        }
    }

    // create class symbol
    auto class_sym = std::make_unique<ClassSymbol>(cls_name, class_type, node.loc);


    // create Class
    semanticCtx.beginScope();
    for (auto &member: fields) {
        if (member) member->accept(*this);

        // Convert VarSymbols to FieldSymbols for class members
        for (auto *var_sym: member->symbols()) {
            if (var_sym) {
                // Create a FieldSymbol with the same properties
                auto field_sym = std::make_unique<FieldSymbol>(
                    var_sym->getName(),
                    var_sym->getType(),
                    var_sym->getLocation()
                );
                field_sym->setDefiningClass(class_sym.get());
                FieldSymbol *raw_field = field_sym.get();

                // Replace the VarSymbol with FieldSymbol in symbol table
                semanticCtx.replaceSymbol(var_sym->getName(), std::move(field_sym));
                class_sym->addField(raw_field);
            }
        }
    }
    for (auto &method: methods) {
        if (method) method->accept(*this);
        auto *func_header = method->funcHeader();
        if (func_header && func_header->symbol()) {
            auto *func_sym = func_header->symbol();

            // Create a MethodSymbol with the same properties
            auto method_sym = std::make_unique<MethodSymbol>(
                func_sym->getName(),
                func_sym->getType(),
                func_sym->isProcedure(),
                func_sym->getLocation()
            );

            // Copy parameters from FuncSymbol to MethodSymbol
            for (auto *param: func_sym->getParams()) {
                method_sym->addParam(param);
            }

            method_sym->setDefiningClass(class_sym.get());
            if (func_sym->isDefined()) {
                method_sym->markDefined();
            }

            MethodSymbol *raw_method = method_sym.get();

            // Replace the FuncSymbol with MethodSymbol in symbol table
            semanticCtx.replaceSymbol(func_sym->getName(), std::move(method_sym));
            // Update the AST node to point to the new MethodSymbol
            func_header->setSymbol(raw_method);
            class_sym->addMethod(raw_method);
        }
    }
    semanticCtx.endScope();
    semanticCtx.declareSymbol(std::move(class_sym));// declare class symbol in current scope
}
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
    if (existing.found()) {
        if (!signaturesMatch(isProcedure, returnType, params, symbol)) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "forward declaration of '" + name + "' conflicts with previous declaration"
            );
            throw std::runtime_error("semantic analysis failed");
        }
        if (symbol->isDefined()) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "symbol '" + name + "' already defined"
            );
            throw std::runtime_error("semantic analysis failed");
        }
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Info,
            Diagnostics::Phase::SemanticAnalysis,
            symbol->getLocation(),
            "previous declaration here"
        );
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

    if (existing.found()) {
        if (!signaturesMatch(isProcedure, returnType, params, symbol)) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "definition of '" + name + "' does not match prior declaration"
            );
            throw std::runtime_error("semantic analysis failed");
        }
        if (symbol->isDefined()) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "redefinition of '" + name + "'"
            );
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
    SemaTypePtr resolved_type = resolveType(*typeNode);
    if (!resolved_type) {
        return;
    }
    // check if var def has initialization and type match
    if (auto &init = node.initExpr(); init.has_value()) {
        Expr *rawptr = init->get();
        if (rawptr) {
            rawptr->accept(*this);
            auto init_type = rawptr->type();
            if (!typesEqual(resolved_type, init_type)) {
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "variable initialization type mismatch: declared type '" +
                        typeToString(resolved_type) +
                        "', but got '" + typeToString(init_type) + "'"
                );
                throw std::runtime_error("semantic analysis failed");
            }
        }
    }
    for (const auto &id: node.identifiers()) {
        auto sym = std::make_unique<VarSymbol>(id, resolved_type, node.loc);
        VarSymbol *raw = sym.get();
        // 将变量符号加载到当前函数帧中
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
void SemanticPass::visit(IfStmt &node) {
    if (auto *cond = node.conditionExpr()) {
        cond->accept(*this);
    }
    if (auto *thenBranch = node.thenBlock()) {
        thenBranch->accept(*this);
    }
    for (auto &elif: node.elifs()) {
        if (elif.first) {
            elif.first->accept(*this);
        }
        if (elif.second) {
            elif.second->accept(*this);
        }
    }
    if (auto *elseBranch = node.elseBlock()) {
        elseBranch->accept(*this);
    }
}
void SemanticPass::visit(LoopStmt &node) {
    if (auto *condition = node.conditionExpr()) {
        condition->accept(*this);
    }
    if (auto *body = node.loopBody()) {
        body->accept(*this);
    }
}
void SemanticPass::visit(ReturnStmt &node) {
    auto *value = node.returnValue();
    if (value) {
        value->accept(*this);
    }

    auto frame = semanticCtx.currentFunction();
    if (!frame || frame->is_procedure || !value) {
        return;
    }
    if (!typesEqual(frame->return_type, value->type())) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "return type mismatch: expected '" + typeToString(frame->return_type) + "' but got '" + typeToString(value->type()) + "'"
        );
        throw std::runtime_error("semantic analysis failed");
    }
}
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
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis, node.loc,
            "invalid assignment target"
        );
        throw std::runtime_error("semantic analysis failed");
    }
    // if (isArrayType(leftType)) {
    //     semanticCtx.getDiagnostics().report(
    //         Diagnostics::Severity::Error,
    //         Diagnostics::Phase::SemanticAnalysis,
    //         node.loc,
    //         "cannot assign to an array value"
    //     );
    //     throw std::runtime_error("semantic analysis failed");
    // }
    if (lhs && !lhs->isAssignable()) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "left-hand side of assignment is not assignable"
        );
        throw std::runtime_error("semantic analysis failed");
    }
    if (!typesEqual(leftType, rightType)) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "assignment type mismatch: left is '" + typeToString(leftType) + "', right is '" + typeToString(rightType) + "'"
        );
        throw std::runtime_error("semantic analysis failed");
    }
}
void SemanticPass::visit(BreakStmt &node) { std::cout << "BreakStmt\n"; }
void SemanticPass::visit(ContinueStmt &node) { std::cout << "ContinueStmt\n"; }
void SemanticPass::visit(ProcCall &node) {
    // main function entry
    auto lookup = semanticCtx.lookup(node.identifier());
    Symbol *symbol = lookup.symbol;
    auto *funcSym = (symbol && symbol->getKind() == Symbol::SymKind::FUNC)
                        ? static_cast<FuncSymbol *>(symbol)
                        : nullptr;
    if (!funcSym || !funcSym->isProcedure()) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "unknown procedure '" + node.identifier() + "'"
        );
        node.setFuncSymbol(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }
    node.setFuncSymbol(funcSym);
    const auto &params = funcSym->getParams();
    checkArguments(node.arguments(), params, node.identifier(), node.loc);
}

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
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "arithmetic operands must both be int or byte"
                );
                throw std::runtime_error("semantic analysis failed");
            }
            node.setType(leftType);
            break;
        case BinOp::AndBits:
        case BinOp::OrBits:
            if (!isByteType(leftType) || !typesEqual(leftType, rightType)) {
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "'&' and '|' require byte operands"
                );
                throw std::runtime_error("semantic analysis failed");
            }
            node.setType(makeByteType());
            break;
        case BinOp::And:
        case BinOp::Or:
            if (!isBoolType(leftType) || !typesEqual(leftType, rightType)) {
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "'and' and 'or' require bool operands"
                );
                throw std::runtime_error("semantic analysis failed");
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
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "comparison operands must both be int or byte"
                );
                throw std::runtime_error("semantic analysis failed");
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
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "unary '+' and '-' require int operand"
                );
                throw std::runtime_error("semantic analysis failed");
            }
            node.setType(makeIntType());
            break;
        case UnOp::Not:
            if (!isBoolType(operandType)) {
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    node.loc,
                    "'!' requires byte operand"
                );
                throw std::runtime_error("semantic analysis failed");
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

    // If not found in current scope, check if it's in a method and look in class members
    if (!lookup.found() || !(symbol->isVariable() || symbol->isParameter() || symbol->isField())) {
        auto frame = semanticCtx.currentFunction();
        if (frame && frame->symbol && frame->symbol->isMethod()) {
            auto *methodSym = static_cast<MethodSymbol *>(frame->symbol);
            auto *classSym = methodSym->definingClass();

            if (classSym) {
                // Search in class fields
                const auto &fields = classSym->getFields();
                for (auto *field: fields) {
                    if (field->getName() == node.identifier()) {
                        node.setSymbol(field);
                        node.setType(field->getType());
                        node.setAssignable(true);
                        return;
                    }
                }
            }
        }

        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "unknown variable '" + node.identifier() + "'"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        node.setSymbol(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }
    node.setSymbol(symbol);
    node.setType(symbol->getType());
    node.setAssignable(true);
}
void SemanticPass::visit(IndexLVal &node) {
    auto *base = node.baseExpr();
    auto *index = node.indexExpr();
    if (base) {
        base->accept(*this);
    }
    if (index) {
        index->accept(*this);
    }
    auto baseType = base ? base->type() : SemaTypePtr{};
    if (!isArrayType(baseType)) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "cannot index non-array value"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        throw std::runtime_error("semantic analysis failed");
    }
    if (!index || !isIntType(index->type())) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "array index must be of type int"
        );
        throw std::runtime_error("semantic analysis failed");
    }
    node.setType(static_cast<const ArrayType *>(baseType.get())->elementType());
    node.setAssignable(base ? base->isAssignable() : true);
}

void SemanticPass::visit(MemberAccessLVal &node) {
    auto *obj = node.object();
    if (obj) {
        obj->accept(*this);
    }

    auto objType = obj ? obj->type() : SemaTypePtr{};

    // Check if object type is an instance of a class
    if (!objType || objType->getKind() != SemaType::TypeKind::INST) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "cannot access member of non-class type"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        throw std::runtime_error("semantic analysis failed");
    }

    // Get the class name and look up the class symbol
    auto instType = static_cast<const InstanceType *>(objType.get());
    auto classLookup = semanticCtx.lookup(instType->className());

    if (!classLookup.found() || classLookup.symbol->getKind() != Symbol::SymKind::CLASS) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "class '" + instType->className() + "' not found"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        throw std::runtime_error("semantic analysis failed");
    }

    auto *classSym = static_cast<ClassSymbol *>(classLookup.symbol);

    // Look up the member in the class
    const auto &fields = classSym->getFields();
    for (auto *field: fields) {
        if (field->getName() == node.memberName()) {
            node.setMemberSymbol(field);
            node.setType(field->getType());
            node.setAssignable(true);
            return;
        }
    }

    semanticCtx.getDiagnostics().report(
        Diagnostics::Severity::Error,
        Diagnostics::Phase::SemanticAnalysis,
        node.loc,
        "member '" + node.memberName() + "' not found in class '" + instType->className() + "'"
    );
    node.setType(nullptr);
    node.setAssignable(false);
    throw std::runtime_error("semantic analysis failed");
}

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
void SemanticPass::visit(FuncCall &node) {
    LookupResult lookup = semanticCtx.lookup(node.identifier());
    Symbol *symbol = lookup.symbol;
    auto *funcSym = (lookup.found() && symbol->getKind() == Symbol::SymKind::FUNC)
                        ? static_cast<FuncSymbol *>(symbol)
                        : nullptr;
    if (!funcSym || funcSym->isProcedure()) {
        semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "unknown function '" + node.identifier() + "'");
        node.setFuncSymbol(nullptr);
        node.setType(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }
    node.setFuncSymbol(funcSym);
    const auto &params = funcSym->getParams();
    checkArguments(node.arguments(), params, node.identifier(), node.loc);
    const auto *sig = static_cast<const FuncType *>(funcSym->getType().get());
    node.setType(sig ? sig->returnType() : SemaTypePtr{});
    node.setLValue(false);
    node.setAssignable(false);
}

void SemanticPass::visit(MemberAccessExpr &node) {
    auto *obj = node.object();
    if (obj) {
        obj->accept(*this);
    }

    auto objType = obj ? obj->type() : SemaTypePtr{};

    // Check if object type is an instance of a class
    if (!objType || objType->getKind() != SemaType::TypeKind::INST) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "cannot access member of non-class type"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        throw std::runtime_error("semantic analysis failed");
    }

    // Get the class name and look up the class symbol
    auto instType = static_cast<const InstanceType *>(objType.get());
    auto classLookup = semanticCtx.lookup(instType->className());

    if (!classLookup.found() || classLookup.symbol->getKind() != Symbol::SymKind::CLASS) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "class '" + instType->className() + "' not found"
        );
        node.setType(nullptr);
        node.setAssignable(false);
        throw std::runtime_error("semantic analysis failed");
    }

    auto *classSym = static_cast<ClassSymbol *>(classLookup.symbol);

    // Look up the member (field or method) in the class
    const auto &fields = classSym->getFields();
    for (auto *field: fields) {
        if (field->getName() == node.memberName()) {
            node.setMemberSymbol(field);
            node.setType(field->getType());
            node.setLValue(true);
            node.setAssignable(true);
            return;
        }
    }

    semanticCtx.getDiagnostics().report(
        Diagnostics::Severity::Error,
        Diagnostics::Phase::SemanticAnalysis,
        node.loc,
        "member '" + node.memberName() + "' not found in class '" + instType->className() + "'"
    );
    node.setType(nullptr);
    node.setAssignable(false);
    throw std::runtime_error("semantic analysis failed");
}

void SemanticPass::visit(MethodCall &node) {
    auto *obj = node.object();
    if (obj) {
        obj->accept(*this);
    }

    auto objType = obj ? obj->type() : SemaTypePtr{};

    // Check if object type is an instance of a class
    if (!objType || objType->getKind() != SemaType::TypeKind::INST) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "cannot call method on non-class type"
        );
        node.setType(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }

    // Get the class name and look up the class symbol
    auto instType = static_cast<const InstanceType *>(objType.get());
    auto classLookup = semanticCtx.lookup(instType->className());

    if (!classLookup.found() || classLookup.symbol->getKind() != Symbol::SymKind::CLASS) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "class '" + instType->className() + "' not found"
        );
        node.setType(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }

    auto *classSym = static_cast<ClassSymbol *>(classLookup.symbol);

    // Look up the method in the class
    const auto &methods = classSym->getMethods();
    MethodSymbol *methodSym = nullptr;
    for (auto *method: methods) {
        if (method->getName() == node.methodName()) {
            methodSym = method;
            break;
        }
    }

    if (!methodSym) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            node.loc,
            "method '" + node.methodName() + "' not found in class '" + instType->className() + "'"
        );
        node.setType(nullptr);
        throw std::runtime_error("semantic analysis failed");
    }

    node.setMethodSymbol(methodSym);
    const auto &params = methodSym->getParams();
    checkArguments(node.arguments(), params, node.methodName(), node.loc);

    const auto *sig = static_cast<const FuncType *>(methodSym->getType().get());
    node.setType(sig ? sig->returnType() : SemaTypePtr{});
    node.setLValue(false);
    node.setAssignable(false);
}

void SemanticPass::visit(ArrayExpr &node) {
    const auto &elems = node.getElements();
    SemaTypePtr elemType = nullptr;

    if (elems.empty()) {
        return;
    }

    for (const auto &e: elems) {
        e->accept(*this);
    }
    elemType = elems[0]->type();
    for (size_t i = 1; i < elems.size(); ++i) {
        if (!typesEqual(elemType, elems[i]->type())) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                node.loc,
                "Array elements must have the same type"
            );
        }
    }
    node.setType(makeArrayType(elemType, elems.size()));
}

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
    // if (dynamic_cast<TrueConst *>(node.expression()) ||
    //     dynamic_cast<FalseConst *>(node.expression())) {
    //     node.setType(makeBoolType());
    //     return;
    // }
    // semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, node.loc, "bare expression cannot be used as condition");
    node.setType(makeBoolType());
}

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
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            mainFunc->loc,
            "No 'main' function defined."
        );
        throw std::runtime_error("semantic analysis failed");
    }
    mainFunc->setEntrypoint(true);
    auto *header = mainFunc->funcHeader();
    if (!header) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            mainFunc->loc,
            "'main' function has no header."
        );
        throw std::runtime_error("semantic analysis failed");
    }
    FuncSymbol *mainSymbol = header->symbol();
    if (!mainSymbol) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            mainFunc->loc,
            "'main' function has no associated symbol."
        );
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
bool SemanticPass::arrayTypesCompatible(const ArrayType *actual, const ArrayType *expected) {
    if (!actual || !expected) return false;

    const auto expectedSize = expected->size();
    const auto actualSize = actual->size();

    // If the parameter is sized, require an exact match from the caller
    if (expectedSize) {
        if (!actualSize || *actualSize != *expectedSize) {
            return false;
        }
    }
    // If the parameter is unsized, any actual size is acceptable
    return typesCompatible(actual->elementType(), expected->elementType());
}
bool SemanticPass::typesCompatible(const SemaTypePtr &actual, const SemaTypePtr &expected) {
    if (actual == expected) return true;
    if (!actual || !expected) return false;

    if (expected->getKind() == SemaType::TypeKind::ARRAY) {
        if (actual->getKind() != SemaType::TypeKind::ARRAY) {
            return false;
        }
        return arrayTypesCompatible(
            static_cast<const ArrayType *>(actual.get()),
            static_cast<const ArrayType *>(expected.get())
        );
    }

    // For non-array types, fall back to structural equality
    return typesEqual(actual, expected);
}

bool SemanticPass::validateDimension(const std::optional<int> &dim, bool allowUnsized, const Location &loc) {
    if (!dim.has_value()) {
        if (allowUnsized) {
            return true;
        } else {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                loc,
                "Array dimension must be specified."
            );
            // return false;
            throw std::runtime_error("semantic analysis failed");
        }
    }
    if (*dim <= 0) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            loc,
            "Array dimension must be a positive integer."
        );
        // return false;
        throw std::runtime_error("semantic analysis failed");
    }
    return true;
}
SemaTypePtr SemanticPass::buildArrayType(const Location &loc, SemaTypePtr base, const vec<std::optional<int>> &dims, bool allowUnsizedFirst) {
    SemaTypePtr currentType = std::move(base);
    //! Attention: build from innermost to outermost
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
    auto array_type = buildArrayType(node.loc, base_type, node.dimensions(), allowUnsizedFirst);
    if (!array_type && node.data_type() == DataType::DataType::MAY_INSTANCE) {
        return makeInstanceType(node.getName().value());
    }
    return array_type;
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
        case SemaType::TypeKind::INST: {
            const InstanceType *instType = static_cast<const InstanceType *>(type.get());
            return "instance of " + instType->className();
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
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                param->loc,
                "Parameter in function '" + name + "' has no type."
            );
            throw std::runtime_error("semantic analysis failed");
        }
        Symbol::ParamPass pass;
        auto resolvedType = resolveParamType(*parm_type, pass);
        if (!resolvedType) {
            continue;
        }

        for (const auto &param_name: param->names()) {
            if (seen.insert(param_name).second == false) {
                semanticCtx.getDiagnostics().report(
                    Diagnostics::Severity::Error,
                    Diagnostics::Phase::SemanticAnalysis,
                    param->loc,
                    "Duplicate parameter name '" + param_name + "' in function '" + name + "'."
                );
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
bool SemanticPass::signaturesMatch(bool isProcedure, const SemaTypePtr &returnType, const std::vector<ParamInfo> &params, const Symbol *symbol) {
    // make sure that function call is matched with definition or declaration
    // often for forward declaration checking and function definition checking
    if (!symbol || symbol->getKind() != Symbol::SymKind::FUNC) {
        return false;
    }
    auto *funcSym = static_cast<const FuncSymbol *>(symbol);
    if (funcSym->isProcedure() != isProcedure) {
        return false;
    }

    // build expected signature for params
    vec<SemaTypePtr> expectedParamTypes;
    expectedParamTypes.reserve(params.size());
    for (const auto &p: params) {
        expectedParamTypes.push_back(p.type);
    }
    auto expectedSig = makeFuncType(returnType, std::move(expectedParamTypes));
    auto actualSig = funcSym->getType();
    if (!typesEqual(expectedSig, actualSig)) {
        return false;
    }
    // check parameter pass modes and count
    const auto &actualParams = funcSym->getParams();
    if (actualParams.size() != params.size()) {
        return false;
    }
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (actualParams[i]->getPass() != params[i].passMode) {
            return false;
        }
        if (!typesEqual(actualParams[i]->getType(), params[i].type)) {
            return false;
        }
    }

    return true;
}
bool SemanticPass::checkArguments(const vec<uptr<Expr>> &args, const std::vector<ParamSymbol *> &params, const std::string &callee, const Location &loc) {
    if (args.size() != params.size()) {
        semanticCtx.getDiagnostics().report(
            Diagnostics::Severity::Error,
            Diagnostics::Phase::SemanticAnalysis,
            loc,
            "call to '" + callee + "' expects " + std::to_string(params.size()) + " argument(s) but got " + std::to_string(args.size())
        );
        throw std::runtime_error("semantic analysis failed");
    }
    std::size_t count = std::min(args.size(), params.size());

    for (std::size_t i = 0; i < count; ++i) {
        auto *arg = args[i].get();
        if (arg) {
            arg->accept(*this);
        }
        auto actualType = arg ? arg->type() : SemaTypePtr{};
        if (!typesCompatible(actualType, params[i]->getType())) {
            semanticCtx.getDiagnostics().report(
                Diagnostics::Severity::Error,
                Diagnostics::Phase::SemanticAnalysis,
                arg ? arg->loc : loc,
                "in call to '" + callee + "', argument " + std::to_string(i + 1) + " has type '" + typeToString(actualType) + "', expected '" + typeToString(params[i]->getType()) + "'"
            );
            throw std::runtime_error("semantic analysis failed");
        }
        if (params[i]->getPass() == Symbol::ParamPass::BY_REF) {
            if (!arg || !arg->isLValue()) {
                semanticCtx.getDiagnostics().report(Diagnostics::Severity::Error, Diagnostics::Phase::SemanticAnalysis, arg ? arg->loc : loc, "in call to '" + callee + "', argument " + std::to_string(i + 1) + " must be an l-value for by-ref parameter");
                throw std::runtime_error("semantic analysis failed");
            }
        }
    }
    for (std::size_t i = count; i < args.size(); ++i) {
        if (auto *arg = args[i].get()) {
            arg->accept(*this);
        }
    }
    return args.size() == params.size();
}
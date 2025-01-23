#ifndef IRGEN_HPP
#define IRGEN_HPP

#include "./Environment.hpp"
#include "./Expr.hpp"
#include "./Stmt.hpp"
#include "llvm-14/llvm/IR/Constants.h"
#include <llvm-14/llvm/IR/GlobalVariable.h>
#include <llvm-14/llvm/IR/IRBuilder.h>
#include <llvm-14/llvm/IR/IRBuilderFolder.h>
#include <llvm-14/llvm/IR/LLVMContext.h>
#include <llvm-14/llvm/IR/LegacyPassManager.h>
#include <llvm-14/llvm/IR/Type.h>
#include <llvm-14/llvm/IR/Value.h>
#include <memory>
#include <stdexcept>
#include <vector>

using Env = std::shared_ptr<Environment>;

// Generic binary operator:
// left and right should have the same type
#define GEN_BINARY_OP(Op, varName)                    \
    do {                                              \
        auto left = evaluate(expr->left);             \
        auto right = evaluate(expr->right);           \
        auto val = builder->Op(left, right, varName); \
        return Object::make_llvmval_obj(val);         \
    } while (false)

// class information
struct ClassInfo {
    llvm::StructType *cls;
    llvm::StructType *parent;
    std::map<std::string, llvm::Type *> fieldsMap;
    std::map<std::string, llvm::Function *> methodsMap;
};

static const size_t RESERVED_FIELD_COUNT = 1;
static const size_t VTABLE_INDEX = 0;

class IRGenerator : public Visitor<Object>,
                    public Visitor_Stmt,
                    public std::enable_shared_from_this<IRGenerator> {
public:
    IRGenerator() {
        moduleInit();
        setupExternalFunctions();
        setupGlobalEnvironment();
        setupTargetTriple();
    };
    ~IRGenerator() = default;

    class EnvironmentGuard {
    public:
        EnvironmentGuard(IRGenerator &ir_generator, Env env);

        ~EnvironmentGuard();

    private:
        IRGenerator &ir_generator;
        std::shared_ptr<Environment> previous_env;
    };

    void compileAndSave(vector<shared_ptr<Stmt>> &statements);

private:
    void compile(vector<shared_ptr<Stmt>> &statements);
    void saveModuleToFile(const std::string &fileName);
    void moduleInit();            // init module and context
    void setupExternalFunctions();// setup external functions
    void setupGlobalEnvironment();// setup global environment
    void setupTargetTriple();     // setup target triple

    // create somthing(object)
    llvm::Function *createFunction(const std::string &fnName, llvm::FunctionType *fnType, Env env);     // create a function
    llvm::Function *createFunctionProto(const std::string &fnName, llvm::FunctionType *fnType, Env env);// create a function prototype
    llvm::BasicBlock *createBB(const std::string &name, llvm::Function *fn);                            // create a basic block
    void createFunctionBlock(llvm::Function *fn);                                                       // create a function block

    llvm::Value *allocVar(const std::string &name, llvm::Type *type, Env env);// allocate a variable

    llvm::GlobalVariable *createGlobalVariable(const std::string &name, llvm::Constant *init);// create a global variable
    llvm::Value *createInstance(shared_ptr<Call<Object>> expr, const std::string &varName);   // create instance
    llvm::Value *mallocInstance(llvm::StructType *cls, const std::string &name);              // allocate an object of a given class on the heap
    llvm::Value *createList(shared_ptr<List<Object>> expr, string elemType);                  // create a list

    // extract type
    llvm::Type *excrateVarType(std::shared_ptr<Expr<Object>> expr);        // extract type from expression
    llvm::Type *excrateTypeByName(const string &typeName, const Token tok);// extract type from string
    llvm::FunctionType *excrateFunType(shared_ptr<Function> stmt);         // extract function type

    size_t getTypeSize(llvm::Type *type);
    llvm::StructType *getClassByName(const std::string &name);// get class by name

    // methods related to class
    void inheritClass(llvm::StructType *cls, llvm::StructType *parent);         // inherit parent class field
    void buildClassInfo(llvm::StructType *cls, const Class &stmt, Env env);     // build class info
    void buildClassBody(llvm::StructType *cls);                                 // build class body
    void buildVTable(llvm::StructType *cls, ClassInfo *classInfo);              // build vtable
    size_t getFieldIndex(llvm::StructType *cls, const std::string &fieldName);  // get field index
    size_t getMethodIndex(llvm::StructType *cls, const std::string &methodName);// get method index
    bool isMethod(llvm::StructType *cls, const std::string &name);              // check if a function is a method
    bool isInstanceArgument(shared_ptr<Expr<Object>> expr);

    //runner functon
    // generate IR for statements
    void codegenerate(vector<shared_ptr<Stmt>> &statements);
    void executeBlock(vector<shared_ptr<Stmt>> statements, Env env);
    void execute(shared_ptr<Stmt> stmt);
    llvm::Value *evaluate(shared_ptr<Expr<Object>> expr);
    llvm::Value *gen(vector<shared_ptr<Stmt>> &statements);// generate IR

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

public:
    // get functions to return private members
    std::unique_ptr<llvm::Module> getModule() {
        return std::move(module);
    }

    std::unique_ptr<llvm::LLVMContext> getContext() {
        return std::move(ctx);
    }

private:
    static llvm::Value *lastValue;                 // last value generated
    std::vector<llvm::Value *> Values;             // all IR values
    Env globalEnv;                                 // global environment
    Env &environment = globalEnv;                  // current env
    llvm::Function *fn;                            // current compiling function
    std::unique_ptr<llvm::LLVMContext> ctx;        // container for modules and other LLVM objects
    std::unique_ptr<llvm::Module> module;          // container for functions and global variables
    std::unique_ptr<llvm::IRBuilder<>> varsBuilder;// this builder always prepends to the beginning of the function entry block
    std::unique_ptr<llvm::IRBuilder<>> builder;    // enter at the end of the function entry block
    llvm::StructType *cls = nullptr;               // current compiling class type
    std::map<std::string, ClassInfo> classMap_;    // class map

    // optimization
    std::unique_ptr<llvm::legacy::FunctionPassManager> fpm;// function pass manager
};
#endif
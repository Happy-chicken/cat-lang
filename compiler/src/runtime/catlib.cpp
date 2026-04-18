#include "catlib.hpp"


#include <cstddef>
#include <cstdio>
#include <llvm-20/llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

extern "C" void cat_print(int x) {
  std::printf("%d\n", x);
}
namespace Catime {

  // Local helper structs for builtin declaration
  struct ParamInfo {
    std::string name;
    Location loc = Location::builtIn();
    SemaTypePtr type;
    Symbol::ParamPass passMode = Symbol::ParamPass::BY_VAL;
  };

  struct HeaderInfo {
    std::string name;
    Location loc = Location::builtIn();
    bool isProcedure = false;
    SemaTypePtr returnType;
    std::vector<ParamInfo> params;
  };

  void declareBuiltins(SemanticCtx &semCtx) {
    HeaderInfo info;
    ParamInfo param;
    for (std::size_t i = 0; i < sizeof(builtinTable) / sizeof(CatBuiltin); ++i) {
      switch (i) {
        case 0: {
          info.params.clear();
          param.type = makeIntType();
          param.name = "^@byte";
          param.passMode = Symbol::ParamPass::BY_VAL;
          info.params.push_back(param);
          info.name = builtinTable[i].catName;
          info.isProcedure = true;
          info.returnType = makeVoidType();
          break;
        }
      }
    }
    std::vector<SemaTypePtr> paramTypes;
    paramTypes.reserve(info.params.size());
    for (const auto &p: info.params) {
      paramTypes.push_back(p.type);
    }
    auto sig = makeFuncType(info.returnType, std::move(paramTypes));
    auto func = std::make_unique<FuncSymbol>(info.name, std::move(sig), info.isProcedure, info.loc);

    semCtx.beginScope();
    for (const auto &pInfo: info.params) {
      auto p = std::make_unique<ParamSymbol>(pInfo.name, pInfo.type, pInfo.passMode, pInfo.loc);
      p->setDefiningFunc(func.get());
      auto res = semCtx.declareSymbol(std::move(p));
      if (res.symbol) {
        func->addParam(static_cast<ParamSymbol *>(res.symbol));
      }
    }
    semCtx.endScope();

    semCtx.declareSymbol(std::move(func), /*reportDuplicates=*/false);
  }
  void genBuiltins(SemanticCtx &semCtx, CodeGen &codegen) {
    auto &ctx = codegen.getContext();
    auto &module = ctx.getModule();
    auto &llctx = ctx.getLLVMContext();
    auto globalEnv = codegen.getGlobalEnvironment();

    auto *i32Ty = llvm::Type::getInt32Ty(llctx);
    auto *i8Ty = llvm::Type::getInt8Ty(llctx);
    auto *ptrTy = llvm::PointerType::get(llctx, 0);

    auto bind = [&](const char *catName, llvm::Function *fn) {
      if (!fn) return;
      auto res = semCtx.lookup(catName);
      if (res.symbol && res.symbol->getKind() == Symbol::SymKind::FUNC) {
        globalEnv->bindFunc(static_cast<FuncSymbol *>(res.symbol), fn);
      }
    };
    llvm::FunctionType *printTy = llvm::FunctionType::get(llvm::Type::getVoidTy(llctx), {i32Ty}, false);
    bind("print", llvm::Function::Create(printTy, llvm::Function::ExternalLinkage, builtinTable[0].runtimeName, &module));
  }
}// namespace Catime

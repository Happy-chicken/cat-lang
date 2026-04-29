#include "catlib.hpp"


#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <llvm-20/llvm/IR/DebugInfoMetadata.h>
#include <llvm-20/llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

extern "C" void cat_print(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
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
    bool isVariadic = false;
    SemaTypePtr returnType;
    std::vector<ParamInfo> params;
  };

  void declareBuiltins(SemanticCtx &semCtx) {
    for (std::size_t i = 0; i < sizeof(builtinTable) / sizeof(CatBuiltin); ++i) {
      HeaderInfo info;
      ParamInfo param;
      switch (i) {
        case 0: {
          info.params.clear();
          info.params.push_back({});
          info.name = builtinTable[i].catName;
          info.isProcedure = true;
          info.isVariadic = true;
          info.returnType = makeVoidType();
          break;
        }
        default:
          continue;
      }
      std::vector<SemaTypePtr> paramTypes;
      paramTypes.reserve(info.params.size());
      for (const auto &p: info.params) {
        paramTypes.push_back(p.type);
      }
      auto sig = makeFuncType(info.returnType, std::move(paramTypes));
      auto func = std::make_unique<FuncSymbol>(info.name, std::move(sig), info.isProcedure, info.isVariadic, info.loc);

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
        auto res_func = static_cast<FuncSymbol *>(res.symbol);
        res_func->setBuiltin(true);
        globalEnv->bindFunc(res_func, fn);
      }
    };
    llvm::FunctionType *printTy = llvm::FunctionType::get(llvm::Type::getVoidTy(llctx), ptrTy, true);
    bind("print", llvm::Function::Create(printTy, llvm::Function::ExternalLinkage, builtinTable[0].runtimeName, &module));
  }
}// namespace Catime

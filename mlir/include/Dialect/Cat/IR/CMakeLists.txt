set(LLVM_TARGET_DEFINITIONS CatDialect.td)
set(LLVM_TARGET_DEFINITIONS CatTypes.td)
set(LLVM_TARGET_DEFINITIONS CatEnums.td)
set(LLVM_TARGET_DEFINITIONS CatAttrs.td)
# 生成NorthStar Dialect 的decl and def
mlir_tablegen(CatDialect.h.inc --gen-dialect-decls -dialect=cat)
mlir_tablegen(CatDialect.cpp.inc --gen-dialect-defs -dialect=cat)

# 生成NorthStar Type 的decl and def
mlir_tablegen(CatTypes.h.inc -gen-typedef-decls -dialect=cat)
mlir_tablegen(CatTypes.cpp.inc -gen-typedef-defs -dialect=cat)

mlir_tablegen(CatEnums.h.inc -gen-enum-decls -dialect=cat)
mlir_tablegen(CatEnums.cpp.inc -gen-enum-defs -dialect=cat)

mlir_tablegen(CatAttrs.h.inc -gen-attrdef-decls -dialect=cat)
mlir_tablegen(CatAttrs.cpp.inc -gen-attrdef-defs -dialect=cat)

# 将生成的命令定义为为target
add_public_tablegen_target(CatDialectIncGen)

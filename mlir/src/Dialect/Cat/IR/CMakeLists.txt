add_mlir_dialect_library(MLIRCatDialect
    CatDialect.cpp
    CatTypes.cpp

    DEPENDS
    CatDialectIncGen

    LINK_LIBS PUBLIC
    MLIRIR
    MLIRTensorDialect
)


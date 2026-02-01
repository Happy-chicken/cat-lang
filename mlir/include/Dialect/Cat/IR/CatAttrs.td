#ifndef DIALECT_CAT_ATTRS_TD
#define DIALECT_CAT_ATTRS_TD
include "mlir/IR/EnumAttr.td"
include "Dialect/Cat/IR/CatEnums.td"
class Cat_Attr<string name, string attrMnemonic, list<Trait> traits = [],
                   string baseCppClass = "::mlir::Attribute">
    : AttrDef<Cat_Dialect, name, traits, baseCppClass> {
  let mnemonic = attrMnemonic;
  let attrName = dialect.name # "." # attrMnemonic;
  let genStorageClass = 1;
  let hasStorageCustomConstructor = 0;
  let skipDefaultBuilders = 0;
  let genVerifyDecl = 0;
}

def Cat_DataParallelism: Cat_Attr<"DataParallelism", "DP", []>{
  let parameters = (ins "int64_t":$DP_nums);
  let assemblyFormat = [{
    `<`
      `DP` `=` $DP_nums
    `>`
  }];
}
#endif

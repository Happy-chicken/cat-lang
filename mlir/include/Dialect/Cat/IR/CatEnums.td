#ifndef DIALECT_CAT_EUNMS_TD
#define DIALECT_CAT_EUNMS_TD
include "mlir/IR/EnumAttr.td"
include "Dialect/Cat/IR/CatTypes.td"

def Cat_LAYOUT_NCHW: I32EnumAttrCase<"NCHW", 0>;
def Cat_LAYOUT_NHWC: I32EnumAttrCase<"NHWC", 1>;

def Cat_Layout : I32EnumAttr<"Layout",
    "Layout of tensor",
    [Cat_LAYOUT_NCHW, Cat_LAYOUT_NHWC]> {
  let genSpecializedAttr = 0;
  let cppNamespace = "::mlir::cat";
}

def LLH_LayoutAttr : EnumAttr<Cat_Dialect, Cat_Layout, "Layout">{
  let assemblyFormat = "`<` $value `>`";

  let extraClassDeclaration = [{
  bool isChannelLast();
  }];
}


#endif

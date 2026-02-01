#include "Dialect/Cat/IR/CatTypes.h"

#include "Dialect/Cat/IR/CatDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/LogicalResult.h"
#include "llvm/Support/raw_ostream.h"
#define FIX
#define GET_TYPEDEF_CLASSES
#include "Dialect/Cat/IR/CatTypes.cpp.inc"

namespace mlir::cat {

    void CatDialect::registerType() {
        llvm::outs() << "register " << getDialectNamespace() << "  Type\n";
        addTypes<
#define GET_TYPEDEF_LIST
#include "Dialect/Cat/IR/CatTypes.cpp.inc"
            >();
    }

    ::llvm::LogicalResult CatTensorType::verify(
        ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError,
        ::llvm::ArrayRef<int64_t> shape, Type elementType, int64_t device_id
    ) {
        if (device_id < 0) {
            return emitError() << " Invalid device id";
        }
        if (!elementType.isIntOrFloat()) {
            return emitError() << " Invalid element type ";
        }
        return llvm::success();
    }

    Type CatTensorType::parse(AsmParser &parser) {
        if (parser.parseLess()) return Type();

        SmallVector<int64_t, 4> dimensions;
        if (parser.parseDimensionList(dimensions, /*allowDynamic=*/true,
                                      /*withTrailingX=*/true))
            return Type();
        // Parse the element type.
        auto typeLoc = parser.getCurrentLocation();
        Type elementType;
        if (parser.parseType(elementType)) return Type();
        // Check that array is formed from allowed types.
        if (parser.parseComma()) return Type();
        int device_id = 0;
        if (parser.parseInteger(device_id))
            if (parser.parseGreater()) return Type();
        return parser.getChecked<CatTensorType>(parser.getContext(), dimensions, elementType, device_id);
    }

    void CatTensorType::print(AsmPrinter &printer) const {
        printer << "<";
        for (int64_t dim: getShape()) {
            if (dim < 0) {
                printer << "?" << 'x';
            } else {
                printer << dim << 'x';
            }
        }
        printer.printType(getElementType());
        printer << ",";
        printer << getDeviceId();
        printer << ">";
    }
}// namespace mlir::cat

#undef FIX

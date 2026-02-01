#include "SemaType.hpp"

#include <cstddef>
#include <ostream>
#include <utility>

SemaType::SemaType(TypeKind kind)
    : kind_(kind) {}

SemaType::TypeKind SemaType::getKind() const {
    return kind_;
}

void SemaType::dump(std::ostream &out) const {
    out << "SemaType(kind=";
    switch (kind_) {
        case TypeKind::INT:
            out << "INT";
            break;
        case TypeKind::CHAR:
            out << "CHAR";
            break;
        case TypeKind::BOOL:
            out << "BOOL";
            break;
        case TypeKind::BYTE:
            out << "BYTE";
            break;
        case TypeKind::STR:
            out << "STR";
            break;
        case TypeKind::ARRAY:
            out << "ARRAY";
            break;
        case TypeKind::FUNC:
            out << "FUNC";
            break;
        case TypeKind::CLS:
            out << "CLS";
            break;
        case TypeKind::INST: {
            const InstanceType *inst_type = static_cast<const InstanceType *>(this);
            out << "INST Of " << inst_type->className();
            break;
        }
        case TypeKind::VOID:
            out << "VOID";
            break;
    }
    out << ")";
}

bool IntType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::INT;
}

bool BoolType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::BOOL;
}

bool CharType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::CHAR;
}

bool StrType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::STR;
}

bool ByteType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::BYTE;
}

ArrayType::ArrayType(SemaTypePtr elementType, std::optional<std::size_t> size)
    : SemaType(TypeKind::ARRAY),
      elem_(std::move(elementType)),
      size_(size) {}

const SemaTypePtr &ArrayType::elementType() const {
    return elem_;
}

std::optional<std::size_t> ArrayType::size() const {
    return size_;
}

bool ArrayType::equals(const SemaType &other) const {
    if (other.getKind() != TypeKind::ARRAY) {
        return false;
    }

    const auto &rhs = static_cast<const ArrayType &>(other);
    if (size_ != rhs.size_) {
        return false;
    }

    if (elem_ == rhs.elem_) {
        return true;
    }

    if (!elem_ || !rhs.elem_) {
        return false;
    }

    return elem_->equals(*rhs.elem_);
}

FuncType::FuncType(SemaTypePtr returnType, std::vector<SemaTypePtr> params)
    : SemaType(TypeKind::FUNC),
      ret_(std::move(returnType)),
      params_(std::move(params)) {}

const SemaTypePtr &FuncType::returnType() const {
    return ret_;
}

const std::vector<SemaTypePtr> &FuncType::params() const {
    return params_;
}

bool FuncType::equals(const SemaType &other) const {
    if (other.getKind() != TypeKind::FUNC) {
        return false;
    }

    const auto &rhs = static_cast<const FuncType &>(other);

    if (ret_ != rhs.ret_) {
        if (!ret_ || !rhs.ret_) {
            return false;
        }
        if (!ret_->equals(*rhs.ret_)) {
            return false;
        }
    }

    if (params_.size() != rhs.params_.size()) {
        return false;
    }

    for (std::size_t i = 0; i < params_.size(); ++i) {
        const auto &lhsParam = params_[i];
        const auto &rhsParam = rhs.params_[i];

        if (lhsParam != rhsParam) {
            if (!lhsParam || !rhsParam) {
                return false;
            }
            if (!lhsParam->equals(*rhsParam)) {
                return false;
            }
        }
    }

    return true;
}

ClassType::ClassType(const std::string &className)
    : SemaType(TypeKind::CLS),
      className_(className) {}

const std::string &ClassType::className() const {
    return className_;
}

bool ClassType::equals(const SemaType &other) const {
    if (other.getKind() != TypeKind::CLS) {
        return false;
    }
    const auto &rhs = static_cast<const ClassType &>(other);
    return className_ == rhs.className_;
}

InstanceType::InstanceType(const std::string &className)
    : SemaType(TypeKind::INST),
      className_(className) {}

const std::string &InstanceType::className() const {
    return className_;
}

void InstanceType::setClassName(const std::string &name) {
    className_ = name;
}

bool InstanceType::equals(const SemaType &other) const {
    if (other.getKind() != TypeKind::INST) {
        return false;
    }
    const auto &rhs = static_cast<const InstanceType &>(other);
    return className_ == rhs.className_;
}

bool VoidType::equals(const SemaType &other) const {
    return other.getKind() == TypeKind::VOID;
}

bool typesEqual(const SemaTypePtr &a, const SemaTypePtr &b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return a->equals(*b);
}

namespace {
    template<class T, class... Args>
    SemaTypePtr makeType(Args &&...args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
}// namespace

SemaTypePtr makeIntType() {
    static SemaTypePtr instance = makeType<IntType>();
    return instance;
}

SemaTypePtr makeBoolType() {
    static SemaTypePtr instance = makeType<BoolType>();
    return instance;
}

SemaTypePtr makeCharType() {
    static SemaTypePtr instance = makeType<CharType>();
    return instance;
}

SemaTypePtr makeStrType(std::size_t length) {
    static SemaTypePtr instance = makeType<StrType>(length);
    return instance;
}

SemaTypePtr makeByteType() {
    static SemaTypePtr instance = makeType<ByteType>();
    return instance;
}

SemaTypePtr makeVoidType() {
    static SemaTypePtr instance = makeType<VoidType>();
    return instance;
}

SemaTypePtr makeArrayType(SemaTypePtr elementType, std::optional<std::size_t> size) {
    return makeType<ArrayType>(std::move(elementType), size);
}

SemaTypePtr makeFuncType(SemaTypePtr returnType, std::vector<SemaTypePtr> params) {
    return makeType<FuncType>(std::move(returnType), std::move(params));
}

SemaTypePtr makeClassType(const std::string &className) {
    return makeType<ClassType>(className);
}

SemaTypePtr makeInstanceType(const std::string &className) {
    return makeType<InstanceType>(className);
}

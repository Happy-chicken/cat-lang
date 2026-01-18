#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>
class SemaType {
public:
    enum class TypeKind {
        INT,
        CHAR,
        BOOL,
        BYTE,
        STR,
        ARRAY,
        FUNC,
        CLS,
        VOID
    };

    virtual ~SemaType() = default;
    TypeKind getKind() const;
    virtual bool equals(const SemaType &other) const = 0;// for type-checking

    void dump(std::ostream &out) const {
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
            case TypeKind::VOID:
                out << "VOID";
                break;
        }
        out << ")";
    }

protected:
    explicit SemaType(TypeKind kind);

private:
    TypeKind kind_;
};

using SemaTypePtr = std::shared_ptr<const SemaType>;

// ---------- Primitive types ----------

class IntType : public SemaType {
public:
    IntType() : SemaType(TypeKind::INT) {}

    bool equals(const SemaType &other) const override;
};

class BoolType : public SemaType {
public:
    BoolType() : SemaType(TypeKind::BOOL) {}

    bool equals(const SemaType &other) const override;
};

class CharType : public SemaType {
public:
    CharType() : SemaType(TypeKind::CHAR) {}

    bool equals(const SemaType &other) const override;
};

class StrType : public SemaType {
public:
    StrType(std::size_t length) : SemaType(TypeKind::STR), length_(length) {}

    bool equals(const SemaType &other) const override;

private:
    std::size_t length_;
};


class ByteType : public SemaType {
public:
    ByteType() : SemaType(TypeKind::BYTE) {}

    bool equals(const SemaType &other) const override;
};

// ---------- Array type ----------

class ArrayType : public SemaType {
public:
    ArrayType(SemaTypePtr elementType, std::optional<std::size_t> size);

    const SemaTypePtr &elementType() const;
    std::optional<std::size_t> size() const;

    bool equals(const SemaType &other) const override;

private:
    SemaTypePtr elem_;
    std::optional<std::size_t> size_;
};

/* Function types refer to their signature type
 * Reminder: two functions/procedures have the same type
 * if they have the same signature
 * */

// ---------- Function type ----------

class FuncType : public SemaType {
public:
    FuncType(SemaTypePtr returnType, std::vector<SemaTypePtr> params);

    const SemaTypePtr &returnType() const;
    const std::vector<SemaTypePtr> &params() const;

    bool equals(const SemaType &other) const override;

private:
    SemaTypePtr ret_;
    std::vector<SemaTypePtr> params_;
};

// ---------- Void type (for procs / "no value") / used for return only ----------

class VoidType : public SemaType {
public:
    VoidType() : SemaType(TypeKind::VOID) {}

    bool equals(const SemaType &other) const override;
};

SemaTypePtr makeIntType();
SemaTypePtr makeBoolType();
SemaTypePtr makeCharType();
SemaTypePtr makeStrType(std::size_t length = 0);
SemaTypePtr makeByteType();
SemaTypePtr makeVoidType();
SemaTypePtr makeArrayType(SemaTypePtr elementType, std::optional<std::size_t> size);
SemaTypePtr makeFuncType(SemaTypePtr returnType, std::vector<SemaTypePtr> params);

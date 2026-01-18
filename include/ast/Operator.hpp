//operators -- purely syntax-related enums and functions for operator names
#pragma once
enum class UnOp {
    Plus,
    Minus,
    Not
};
enum class BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    AndBits,
    OrBits,
    Eq,
    Ne,
    Le,
    Ge,
    Lt,
    Gt,
    And,
    Or
};


inline const char *unOpName(UnOp op) {
    switch (op) {
        case UnOp::Plus:
            return "+";
        case UnOp::Minus:
            return "-";
        case UnOp::Not:
            return "!";
    }
    return "?";
}

inline const char *binOpName(BinOp op) {
    switch (op) {
        case BinOp::Add:
            return "+";
        case BinOp::Sub:
            return "-";
        case BinOp::Mul:
            return "*";
        case BinOp::Div:
            return "/";
        case BinOp::Mod:
            return "%";
        case BinOp::AndBits:
            return "&";
        case BinOp::OrBits:
            return "|";
        case BinOp::Eq:
            return "=";
        case BinOp::Ne:
            return "!=";
        case BinOp::Le:
            return "<=";
        case BinOp::Ge:
            return ">=";
        case BinOp::Lt:
            return "<";
        case BinOp::Gt:
            return ">";
        case BinOp::And:
            return "and";
        case BinOp::Or:
            return "or";
    }
    return "?";
}

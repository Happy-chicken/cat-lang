#pragma once
#include <string>
namespace DataType {
    enum class DataType {
        INT,
        FLOAT,
        BOOL,
        CHAR,
        VOID,
        STRING,
        BYTE,
        CLASS,
        UNKOWN
    };

    inline std::string toString(DataType dt) {
        switch (dt) {
            case DataType::INT:
                return "int";
            case DataType::CHAR:
                return "char";
            case DataType::FLOAT:
                return "float";
            case DataType::BOOL:
                return "bool";
            case DataType::VOID:
                return "void";
            case DataType::STRING:
                return "string";
            default:
                return "unknown";
        }
    }
}// namespace DataType
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
        MAY_INSTANCE,
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
            case DataType::MAY_INSTANCE:
                return "instance?";
            default:
                return "unknown";
        }
    }
}// namespace DataType
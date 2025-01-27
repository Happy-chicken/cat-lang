#pragma once

#include <map>
#include <string>
#define OP_HALT 0x0000

#define OP_CONST 0x0001

#define OP_ADD 0x002
#define OP_SUB 0x0003
#define OP_MUL 0x0004
#define OP_DIV 0x0005

#define OP_AND 0x0006
#define OP_OR 0x0007
#define OP_NOT 0x0008

#define OP_COMPARE 0x0009

#define OP_JUMP_IF_FALSE 0x000A
#define OP_JUMP 0x000B

#define OP_CALL 0x000C
#define OP_RETURN 0x000D
#define OP_PRINT 0x000E
#include <cstdint>
// enum class
static std::map<uint8_t, std::string> instructionMap =
    {
        {OP_HALT, "OP_HALT"},
        {OP_CONST, "OP_CONST"},
        {OP_ADD, "OP_ADD"},
        {OP_SUB, "OP_SUB"},
        {OP_MUL, "OP_MUL"},
        {OP_DIV, "OP_DIV"},
        {OP_AND, "OP_AND"},
        {OP_OR, "OP_OR"},
        {OP_NOT, "OP_NOT"},
        {OP_COMPARE, "OP_COMPARE"},
        {OP_JUMP_IF_FALSE, "OP_JUMP_IF_FALSE"},
        {OP_JUMP, "OP_JUMP"},
        {OP_CALL, "OP_CALL"},
        {OP_PRINT, "OP_PRINT"}};

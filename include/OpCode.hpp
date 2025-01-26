#pragma once

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
#define OP_PRINT 0x000D
#include <cstdint>
// enum class
enum class OpCode : uint8_t {
    HALT = OP_HALT,
};

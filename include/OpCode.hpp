#pragma once

#define OP_HALT 0x00
#include <cstdint>
// enum class
enum class OpCode : uint8_t {
    HALT = OP_HALT,
};

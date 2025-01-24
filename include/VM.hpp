#pragma once
#include "ByteCompiler.hpp"
#include "OpCode.hpp"
#include "Stmt.hpp"
#include "Value.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// read a byte from the bytecode
#define READ_BYTE() (*ip++)
// get consant
#define READ_CONSTANT() (codeobj->constants[READ_BYTE()])
// calculate the binary operation
#define BINARY_OP(op)                                                             \
    do {                                                                          \
        auto b = pop();                                                           \
        auto a = pop();                                                           \
        if (a.type == CatVM::ValueType::INT && b.type == CatVM::ValueType::INT) { \
            push(INT(AS_INT(a) op AS_INT(b)));                                    \
        } else {                                                                  \
            DIE << "Operands must be two integers.\n";                            \
        }                                                                         \
    } while (0)
// max stack size
#define STACK_MAX 512

class CatStackVM {
public:
    CatStackVM() : ip(nullptr), sp(nullptr) { compiler = std::make_shared<ByteCompiler>(); };
    CatVM::Value run(std::vector<std::shared_ptr<Stmt>> &statements);
    CatVM::Value eval();

    // --------------------------------------------
    // stack manipulation
    // --------------------------------------------
    void push(const CatVM::Value &value);// push a value onto the stack
    CatVM::Value pop();                  // pop a value from the stack


private:
    // instruction pointer
    uint8_t *ip;
    // stack pointer
    CatVM::Value *sp;
    // code
    CatVM::CodeObject *codeobj;
    // operand stack
    std::array<CatVM::Value, STACK_MAX> stack;
    // compiler
    std::shared_ptr<ByteCompiler> compiler;
};
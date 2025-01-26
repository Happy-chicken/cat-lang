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
#define BINARY_LOGIC_OP(op)                                                   \
    do {                                                                      \
        auto right = pop();                                                   \
        auto left = pop();                                                    \
        if (right.type == left.type && left.type == CatVM::ValueType::BOOL) { \
            push(BOOL(AS_BOOL(left) op AS_BOOL(right)));                      \
        } else {                                                              \
            DIE << "[Runtime]: Non-logical operands.";                        \
        }                                                                     \
    } while (false)

#define BINARY_NUMERIC_OP(op)                                          \
    do {                                                               \
        auto right = pop();                                            \
        auto left = pop();                                             \
        if (right.type == left.type) {                                 \
            switch (right.type) {                                      \
                case CatVM::ValueType::INT:                            \
                    push(INT(AS_INT(left) op AS_INT(right)));          \
                    break;                                             \
                case CatVM::ValueType::DOUBLE:                         \
                    push(DOUBLE(AS_DOUBLE(left) op AS_DOUBLE(right))); \
                default:                                               \
                    DIE << "[Runtime]: Non-numeric operands.";         \
            }                                                          \
        }                                                              \
    } while (false)
#define COMPARE_VALUE(op, left, right)                         \
    do {                                                       \
        switch (op) {                                          \
            case 0:                                            \
                push(BOOL(left == right));                     \
                break;                                         \
            case 1:                                            \
                push(BOOL(left != right));                     \
                break;                                         \
            case 2:                                            \
                push(BOOL(left < right));                      \
                break;                                         \
            case 3:                                            \
                push(BOOL(left <= right));                     \
                break;                                         \
            case 4:                                            \
                push(BOOL(left > right));                      \
                break;                                         \
            case 5:                                            \
                push(BOOL(left >= right));                     \
                break;                                         \
            default:                                           \
                DIE << "[Runtime]: Unknown compare operator."; \
        }                                                      \
    } while (false)

#define READ_SHORT() (ip += 2, (uint16_t) ((ip[-2] << 8) | ip[-1]))

#define TO_ADDRESS(offset) (&codeobj->bytecodes[offset])

#define STACK_MAX 512// max stack size

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

    void dump() {
        for (auto &byte: codeobj->bytecodes) {
            std::cout << (int) byte << " ";
        }
        std::cout << std::endl;
    }


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
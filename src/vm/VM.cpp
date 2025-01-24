#include "VM.hpp"
#include "Logger.hpp"
#include "OpCode.hpp"
#include "Value.hpp"
#include <ios>

CatVM::Value CatStackVM::run(std::vector<std::shared_ptr<Stmt>> &statements) {
    // compile the statements ast into bytecode
    codeobj = compiler->compile(statements);
    // constants.push_back(INT(100));
    ip = &codeobj->bytecodes[0];
    sp = &codeobj->constants[0];
    return eval();
}

CatVM::Value CatStackVM::eval() {
    while (true) {
        auto opcode = READ_BYTE();
        switch (opcode) {
            case OP_HALT:
                return pop();
            case OP_CONST: {
                // auto constIndex = READ_BYTE();
                // auto constant = constants[constIndex];
                push(READ_CONSTANT());
                break;
            }
            // Binary operations
            case OP_ADD: {
                BINARY_OP(+);
                // TODO add string concatenation
                break;
            }
            case OP_SUB: {
                BINARY_OP(-);
                break;
            }
            case OP_MUL: {
                BINARY_OP(*);
                break;
            }
            case OP_DIV: {
                BINARY_OP(/);
                break;
            }
            default:
                DIE << "Unknown opcode: " << std::hex << opcode;
        }
    }
}

void CatStackVM::push(const CatVM::Value &value) {
    // check if the stack is full
    if ((size_t) (sp - stack.begin()) == STACK_MAX) {
        DIE << "Stack overflow.\n";
    }
    // ? *sp fault address
    *sp = value;
    sp++;
}

CatVM::Value CatStackVM::pop() {
    if (sp == stack.begin()) {
        DIE << "Stack is empty!\n";
    }
    --sp;
    return *sp;
}
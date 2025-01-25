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
    sp = &stack[0];
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
                BINARY_NUMERIC_OP(+);
                // check the
                // TODO add string concatenation
                break;
            }
            case OP_SUB: {
                BINARY_NUMERIC_OP(-);
                break;
            }
            case OP_MUL: {
                BINARY_NUMERIC_OP(*);
                break;
            }
            case OP_DIV: {
                BINARY_NUMERIC_OP(/);
                break;
            }
            case OP_AND: {
                BINARY_LOGIC_OP(&&);
                break;
            }
            case OP_OR: {
                BINARY_LOGIC_OP(||);
                break;
            }
            case OP_NOT: {
                auto operand = pop();
                push(BOOL(!AS_BOOL(operand)));
                break;
            }
            case OP_PRINT: {
                push(READ_CONSTANT());// read arg number
                // solve the problem of printing whose args in reverse order
                auto arg_num = pop();
                for (int i = 0; i < AS_INT(arg_num); i++) {
                    auto arg = *(sp - AS_INT(arg_num) + i);
                    if (arg.type == CatVM::ValueType::INT) {
                        std::cout << AS_INT(arg) << " ";
                    } else if (arg.type == CatVM::ValueType::DOUBLE) {
                        std::cout << AS_DOUBLE(arg) << " ";
                    } else if (arg.type == CatVM::ValueType::OBJECT) {
                        std::cout << AS_CPP_STRING(arg) << " ";
                    } else {
                        std::cout << AS_INT(arg) << " ";
                    }
                }
                // sp back to the address below the first arg
                sp -= AS_INT(arg_num);
                break;
            }
            default:
                DIE << "[Runtime]: Unknown opcode: " << std::hex << opcode;
        }
    }
}

void CatStackVM::push(const CatVM::Value &value) {
    // check if the stack is full
    if ((size_t) (sp - stack.begin()) == STACK_MAX) {
        DIE << "[Runtime]: Stack overflow.\n";
    }
    // ? *sp fault address
    *sp = value;
    sp++;
}

CatVM::Value CatStackVM::pop() {
    if (sp == stack.begin()) {
        DIE << "[Runtime]: Stack is empty!\n";
    }
    --sp;
    return *sp;
}
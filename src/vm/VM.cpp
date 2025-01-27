#include "VM.hpp"
#include "Logger.hpp"
#include "OpCode.hpp"
#include "Value.hpp"
#include <ios>
#include <llvm-14/llvm/BinaryFormat/Dwarf.h>

CatVM::Value CatStackVM::run(std::vector<std::shared_ptr<Stmt>> &statements) {
    // compile the statements ast into bytecode
    codeobj = compiler->compile(statements);
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
                // return CatVM::Value();
            case OP_CONST: {
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
            case OP_COMPARE: {
                auto op = READ_BYTE();
                auto right = pop();
                auto left = pop();
                if (IS_INT(left) && IS_INT(right)) {
                    auto left_int = AS_INT(left);
                    auto right_int = AS_INT(right);
                    COMPARE_VALUE(op, left_int, right_int);
                } else if (IS_DOUBLE(left) && IS_DOUBLE(right)) {
                    auto left_double = AS_DOUBLE(left);
                    auto right_double = AS_DOUBLE(right);
                    COMPARE_VALUE(op, left_double, right_double);
                } else if (IS_STRING(left) && IS_STRING(right)) {
                    auto left_string = AS_CPP_STRING(left);
                    auto right_string = AS_CPP_STRING(right);
                    COMPARE_VALUE(op, left_string, right_string);
                } else {
                    DIE << "[Runtime]: Non-comparable operands.";
                }

                break;
            }
            case OP_PRINT: {
                push(READ_CONSTANT());// read arg number
                // solve the problem of printing whose args in reverse order
                auto arg_num = pop();
                for (int i = 0; i < AS_INT(arg_num); i++) {
                    auto arg = *(sp - AS_INT(arg_num) + i);
                    // auto arg = pop();
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
                std::cout << "\n";
                // sp back to the address below the first arg
                sp -= AS_INT(arg_num);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                auto condition = pop();
                auto offset = READ_SHORT();
                if (AS_BOOL(condition) == false) {
                    ip = TO_ADDRESS(offset);
                }
                break;
            }
            case OP_JUMP: {
                auto offset = READ_SHORT();
                ip = TO_ADDRESS(offset);
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
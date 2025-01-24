#include "VM.hpp"
#include "OpCode.hpp"

#define READ_BYTE() (*ip++)

void CatStackVM::run(std::vector<std::shared_ptr<Stmt>> &statements) {
    // run the programe
    // compile the statements ast into bytecode
    // code = compiler->compile(statements);
    // bytecode = {OP_HALT};
    // set the instruction pointer to the first instruction
    ip = &bytecode[0];

    return eval();
}

void CatStackVM::eval() {
    while (true) {
        switch (READ_BYTE()) {
            case OP_HALT:
                return;
        }
    }
}
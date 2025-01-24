#pragma once
#include "OpCode.hpp"
#include "Stmt.hpp"
#include <cstdint>
#include <string>
#include <vector>
class CatStackVM {
public:
    CatStackVM() : bytecode{OP_HALT}, ip(nullptr){};
    void run(std::vector<std::shared_ptr<Stmt>> &statements);
    void eval();

private:
    // instruction pointer
    uint8_t *ip;
    // bytecode
    std::vector<uint8_t> bytecode;
};
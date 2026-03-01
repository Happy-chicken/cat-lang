#include "Assemble.h"
#include <ostream>
namespace dcc {
  unsigned Inst::index = 0;

  std::ostream &
  operator<<(std::ostream &out, BinOp &other) {
    out << "ID=" << other.getID() << " " << other.dst << "=" << other.src0 << other.getOp() << other.src1 << "\n";
    return out;
  }
  Inst *Add::eval(Env &env) {
    auto [dst, src0, src1] = getOperands();
    env.setVar(dst, env.getVar(src0) + env.getVar(src1));
    return getNext();
  }

  Inst *Mul::eval(Env &env) {
    auto [dst, src0, src1] = getOperands();
    env.setVar(dst, env.getVar(src0) * env.getVar(src1));
    return getNext();
  }
  std::ostream &operator<<(std::ostream &out, Bt &other) {
    out << "ID=" << other.getID() << " bt" << other.cond << "\n";
    return out;
  }

  const Env &interpret(Inst *inst, Env &env) {
    while (inst) {
      inst = inst->eval(env);
    }
    return env;
  }
}// namespace dcc

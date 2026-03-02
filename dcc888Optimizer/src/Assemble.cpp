#include "Assemble.h"
#include <cassert>
#include <ostream>
#include <variant>
namespace dcc {
  unsigned Inst::index = 0;

  // x = s.allocaVar(0)
  std::string Storage::allocaVar(unsigned locSite) {
    std::string name = dynLocName(locSite);
    locCounter++;
    storage[name] = std::nullopt;
    return name;
  }

  void Storage::store(std::string name, int value) {
    storage[name] = value;
  }

  std::optional<int> Storage::load(std::string name) {
    return storage[name];
  }

  void Storage::dump() {
    for (auto &[k, v]: storage) {
      std::cout << k << ": " << v.value() << "\n";
    }
  }

  std::ostream &operator<<(std::ostream &out, BinOp &other) {
    out << "ID=" << other.getID() << " " << other.dst << "=" << other.src0 << other.getOp() << other.src1 << "\n";
    return out;
  }
  Inst *Add::eval(Env &env, Storage &storage) {
    auto [dst, src0, src1] = getOperands();
    auto val0 = env.getVar(src0);
    auto val1 = env.getVar(src1);
    assert(std::holds_alternative<int>(val0) && std::holds_alternative<int>(val1));
    env.setVar(dst, std::get<int>(val0) + std::get<int>(val1));
    return getNext();
  }

  Inst *Mul::eval(Env &env, Storage &storage) {
    auto [dst, src0, src1] = getOperands();
    auto val0 = env.getVar(src0);
    auto val1 = env.getVar(src1);
    assert(std::holds_alternative<int>(val0) && std::holds_alternative<int>(val1));
    env.setVar(dst, std::get<int>(val0) * std::get<int>(val1));
    return getNext();
  }
  std::ostream &operator<<(std::ostream &out, Bt &other) {
    out << "ID=" << other.getID() << " bt" << other.cond << "\n";
    return out;
  }

  std::ostream &operator<<(std::ostream &out, Store &other) {
    out << "ID=" << other.getID() << ": " << other.ref << " = store " << other.src;
    return out;
  }
  Inst *Store::eval(Env &env, Storage &storage) {
    auto ref = env.getVar(this->ref);
    assert(std::holds_alternative<std::string>(ref));
    auto ref_str = std::get<std::string>(ref);
    auto src = env.getVar(this->src);
    assert(std::holds_alternative<int>(src));
    storage.store(ref_str, std::get<int>(src));
    return getNext();
  }

  std::ostream &operator<<(std::ostream &out, Load &other) {
    out << "ID=" << other.getID() << ": " << other.src << " = load " << other.ref;
    return out;
  }
  Inst *Load::eval(Env &env, Storage &storage) {
    auto ref = env.getVar(this->ref);
    assert(std::holds_alternative<std::string>(ref));
    auto ref_str = std::get<std::string>(ref);
    auto val = storage.load(ref_str);
    env.setVar(ref_str, val.value());
    return getNext();
  }

  std::ostream &operator<<(std::ostream &out, Alloca &other) {
    out << "ID=" << other.getID() << ": " << other.name << " = alloca ";
    return out;
  }
  Inst *Alloca::eval(Env &env, Storage &storage) {
    auto ref = storage.allocaVar(getID());
    env.setVar(name, ref);
    return getNext();
  }

  std::ostream &operator<<(std::ostream &out, Move &other) {
    out << "ID=" << other.getID() << ": " << other.dst << " =  " << other.src;
    return out;
  }
  Inst *Move::eval(Env &env, Storage &storage) {}

  const Env &interpret(Inst *inst, Env &env, Storage &storage) {
    while (inst) {
      inst = inst->eval(env, storage);
    }
    return env;
  }


}// namespace dcc

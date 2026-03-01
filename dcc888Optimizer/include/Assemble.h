#pragma once
#include <algorithm>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace dcc {
  // R(x)
  class Env {
public:
    Env(const std::unordered_map<std::string, int> &inits) : env(inits) {
    }
    int getVar(const std::string &var) {
      if (env.find(var) != env.end()) {
        return env[var];
      }
      throw "absent key";
    }
    void setVar(const std::string &var, int val) {
      env[var] = val;
    }
    void dump() const {
      for (auto [var, val]: env) {
        std::cout << "var=" << var << ", val=" << val << "\n";
      }
    }

private:
    std::unordered_map<std::string, int> env;
  };
  // simulate mmeory env
  class Storage {
public:
    Storage(const std::unordered_map<std::string, int> &storage) : locCounter{0}, storage{std::move(storage)} {}
    static std::string staticLocName(unsigned siteID) {
      std::stringstream ss;
      ss << "ref_" << std::to_string(siteID);
      return ss.str();
    }
    std::string dynLocName(int siteID) {
      std::stringstream ss;
      ss << "ref_" << std::to_string(siteID) << "_" << std::to_string(locCounter);
      return ss.str();
    }

    void allocaVar(unsigned locSite);
    void store(std::string name, int value);
    int load(std::string name);
    void dump();


private:
    std::unordered_map<std::string, int> storage{};
    unsigned locCounter{0};
  };

  class Inst {
public:
    Inst() : next(nullptr), preds({}), ID(index) {
      index++;
    }
    void addNext(Inst *nextInst) {
      next = nextInst;
      nextInst->addPred(this);
    }
    Inst *getNext() const {
      return next;
    }
    void addPred(Inst *pred) { preds.push_back(pred); }
    const std::vector<Inst *> &getPred() const { return preds; }
    unsigned getID() const { return ID; }
    virtual std::unordered_set<std::string> defintion() = 0;
    virtual std::unordered_set<std::string> uses() = 0;
    virtual Inst *eval(Env &) = 0;
    static unsigned index;

private:
    Inst *next;
    // 前驱只观察，不拥有所有权
    std::vector<Inst *> preds;
    unsigned ID;
  };

  class BinOp : public Inst {
public:
    BinOp(const std::string &dst, const std ::string &src0, const std::string &src1) : dst(dst), src0(src0), src1(src1) {}
    std::unordered_set<std::string> defintion() override {
      return {dst};
    }
    std ::unordered_set<std::string> uses() override {
      return {src0, src1};
    }
    virtual Inst *eval(Env &) override = 0;
    virtual const std::string getOp() const = 0;
    const std::string &getDst() const { return dst; }
    std::tuple<std::string, std::string, std::string> getOperands() const { return {dst, src0, src1}; }

    friend std::ostream &operator<<(std::ostream &out, BinOp &other);

private:
    std::string dst, src0, src1;
  };

  class Add : public BinOp {
public:
    Add(const std::string &dst, const std ::string &src0, const std::string &src1) : BinOp(dst, src0, src1) {}
    Inst *eval(Env &env) override;
    const std::string getOp() const override { return "+"; }
  };

  class Mul : public BinOp {
public:
    Mul(const std::string &dst, const std ::string &src0, const std::string &src1) : BinOp(dst, src0, src1) {}
    Inst *eval(Env &env) override;
    const std::string getOp() const override { return "*"; }
  };

  class Lth : public BinOp {
public:
    Lth(const std::string &dst, const std ::string &src0, const std::string &src1) : BinOp(dst, src0, src1) {}
    Inst *eval(Env &env) override {
      auto [dst, src0, src1] = getOperands();
      env.setVar(dst, env.getVar(src0) < env.getVar(src1) ? 1 : 0);
      return getNext();
    }
    const std::string getOp() const override { return "<"; }
  };

  class Geq : public BinOp {
public:
    Geq(const std::string &dst, const std ::string &src0, const std::string &src1) : BinOp(dst, src0, src1) {}
    Inst *eval(Env &env) override {
      auto [dst, src0, src1] = getOperands();
      env.setVar(dst, env.getVar(src0) >= env.getVar(src1) ? 1 : 0);
      return getNext();
    }
    const std::string getOp() const override { return ">="; }
  };

  class Bt : public Inst {
public:
    Bt(const std::string &cond, Inst *trueDst = nullptr, Inst *falseDst = nullptr) : cond(cond), trueDst(trueDst), falseDst(falseDst) {
      if (trueDst) trueDst->addPred(this);
      if (falseDst) falseDst->addPred(this);
    }
    std::unordered_set<std::string> defintion() override { return {}; }
    std::unordered_set<std::string> uses() override { return {cond}; }

    void addFalseNext(Inst *inst) {
      falseDst = inst;
      falseDst->addPred(this);
    }
    void addTrueNext(Inst *inst) {
      trueDst = inst;
      trueDst->addPred(this);
    }

    Inst *eval(Env &env) override {
      return env.getVar(cond) ? trueDst : falseDst;
    }

    friend std::ostream &operator<<(std::ostream &out, Bt &other);

private:
    std::string cond;
    Inst *trueDst;
    Inst *falseDst;
  };

  const Env &interpret(Inst *inst, Env &env);
}// namespace dcc

#pragma once
#include "Assemble.h"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace dcc {
  using std::string;
  template<typename T>
  using set = std::set<T>;

  template<typename T>
  using vec = std::vector<T>;

  // IN_0: {<'var', idx>, <>}
  using DataFlowEnv = std::map<string, set<std::pair<string, unsigned>>>;

  class DataFlowEquation;
  DataFlowEnv solveEquations(const vec<DataFlowEquation *> &equations);
  DataFlowEnv solveEquationsWithWorkList(const vec<DataFlowEquation *> &equations);
  vec<DataFlowEquation *> genReachingDefConstraints(const vec<Inst *> &insts);

  class DataFlowEquation {
public:
    DataFlowEquation(Inst *inst) : instruction(inst) {
    }
    virtual const string name() const = 0;
    virtual bool eval(DataFlowEnv &dataflowEnv) = 0;
    virtual void dump() = 0;
    virtual vec<string> deps() = 0;

    Inst *getInst() const { return instruction; }

private:
    Inst *instruction;
  };

  class InEquation : public DataFlowEquation {
public:
    InEquation(Inst *inst) : DataFlowEquation(inst) {}
    const string name() const override;

private:
  };

  class OutEquation : public DataFlowEquation {
public:
    OutEquation(Inst *inst) : DataFlowEquation(inst) {}
    const string name() const override;

private:
  };

  // equation of reaching definition for binary operation
  class ReachineDefBin_OutEquation : public OutEquation {
public:
    ReachineDefBin_OutEquation(Inst *inst) : OutEquation(inst) {}
    bool eval(DataFlowEnv &env) override;
    void dump() override;
    vec<string> deps() override;

private:
  };
  // equation of reaching definition for jump
  class ReachineDefBt_OutEquation : public OutEquation {
public:
    ReachineDefBt_OutEquation(Inst *inst) : OutEquation(inst) {}

    bool eval(DataFlowEnv &env) override;
    void dump() override;
    vec<string> deps() override;

private:
  };
  class ReachineDef_InEquation : public InEquation {
public:
    ReachineDef_InEquation(Inst *inst) : InEquation(inst) {}

    bool eval(DataFlowEnv &env) override;
    void dump() override;
    vec<string> deps() override;

private:
  };
}// namespace dcc

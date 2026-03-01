#include "DataFlow.h"
#include "Assemble.h"
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace dcc {
  DataFlowEnv solveEquations(const vec<DataFlowEquation *> &equations) {
    bool changed = true;
    DataFlowEnv env;
    for (auto *eq: equations) {
      env[eq->name()] = {};
    }
    while (changed) {
      changed = false;
      for (auto *eq: equations) {
        changed |= eq->eval(env);
      }
    }
    return env;
  }

  DataFlowEnv solveEquationsWithWorkList(const vec<DataFlowEquation *> &equations) {
    auto buildDepGraph = [equations]() {
      std::map<string, vec<DataFlowEquation *>> depGraph{};
      // init depGraph
      for (auto *eq: equations) {
        depGraph[eq->name()] = {};
      }
      for (auto *eq: equations) {
        for (auto &dep: eq->deps()) {
          if (depGraph.find(dep) != depGraph.end()) {
            depGraph[dep].push_back(eq);
          }
        }
      }
      return depGraph;
    };
    bool changed = true;
    DataFlowEnv env;
    for (auto *eq: equations) {
      env[eq->name()] = {};
    }
    auto depGraph = buildDepGraph();
    std::deque<DataFlowEquation *> workList{};
    workList.insert(workList.begin(), equations.begin(), equations.end());
    while (!workList.empty()) {
      auto eq = workList.front();
      workList.pop_front();
      changed = eq->eval(env);
      if (changed) {
        for (auto *depEq: depGraph[eq->name()]) {
          workList.push_back(depEq);
        }
      }
    }
    return env;
  }
  static const string nameIn(unsigned ID) {
    return "IN_" + std::to_string(ID);
  }

  static const string nameOut(unsigned ID) {
    return "OUT_" + std::to_string(ID);
  }

  vec<DataFlowEquation *> genReachingDefConstraints(const vec<Inst *> &insts) {
    vec<DataFlowEquation *> inEqs;
    for (auto *inst: insts) {
      if (auto binInst = dynamic_cast<BinOp *>(inst)) {
        auto *tmp = new ReachineDefBin_OutEquation(binInst);
        inEqs.push_back(tmp);
      }
    }
    for (auto *inst: insts) {
      if (auto btInst = dynamic_cast<Bt *>(inst)) {
        auto *tmp = new ReachineDefBt_OutEquation(btInst);
        inEqs.push_back(tmp);
      }
    }
    vec<DataFlowEquation *> outEqs;
    for (auto *inst: insts) {
      auto *tmp = new ReachineDef_InEquation(inst);
      outEqs.push_back(tmp);
    }
    inEqs.insert(inEqs.end(), outEqs.begin(), outEqs.end());
    return inEqs;
  }

  vec<DataFlowEquation *> genLivenessConstraints(vec<Inst *> &inst) {
    return {};
  }

  const string InEquation::name() const {
    return nameIn(getInst()->getID());
  }

  const string OutEquation::name() const {
    return nameOut(getInst()->getID());
  }
  void ReachineDefBin_OutEquation::dump() {
    std::stringstream ss;
    ss << name() << ": (" << static_cast<BinOp *>(getInst())->getDst() << ", " << getInst()->getID() << ")";
    ss << " + (" << nameIn(getInst()->getID()) << " - (" << static_cast<BinOp *>(getInst())->getDst() << ", _))";
    std::cout << ss.str();
  }
  vec<string> ReachineDefBin_OutEquation::deps() {
    return {nameIn(getInst()->getID())};
  }
  bool ReachineDefBin_OutEquation::eval(DataFlowEnv &env) {
    auto oldEnv = env[name()];
    auto inSet = env[nameIn(getInst()->getID())];
    set<std::pair<string, unsigned>> newSet;
    for (auto [var, idx]: inSet) {
      if (var != static_cast<BinOp *>(getInst())->getDst()) {
        newSet.insert(std::make_pair(var, idx));
      }
    }
    if (auto binInst = dynamic_cast<BinOp *>(getInst())) {
      newSet.insert(std::make_pair(binInst->getDst(), getInst()->getID()));
    }
    env[name()] = newSet;

    return env[name()] != oldEnv;
  }

  void ReachineDefBt_OutEquation::dump() {
    std::stringstream ss;
    ss << name() << ": " << nameIn(getInst()->getID());
    std::cout << ss.str();
  }
  vec<string> ReachineDefBt_OutEquation::deps() {
    return {nameIn(getInst()->getID())};
  }
  bool ReachineDefBt_OutEquation::eval(DataFlowEnv &env) {
    auto oldEnv = env[name()];
    env[name()] = env[nameIn(getInst()->getID())];
    return env[name()] != oldEnv;
  }

  void ReachineDef_InEquation::dump() {
    std::stringstream ss;
    ss << name() << ": Union(";
    for (auto *pred: getInst()->getPred()) {
      ss << " " << nameOut(pred->getID());
    }
    ss << ")";
    std::cout << ss.str();
  }
  vec<string> ReachineDef_InEquation::deps() {
    vec<string> depList{};
    for (auto *pred: getInst()->getPred()) {
      depList.emplace_back(nameOut(pred->getID()));
    }
    return depList;
  }
  bool ReachineDef_InEquation::eval(DataFlowEnv &env) {
    set<std::pair<string, unsigned>> solution{};
    auto oldEnv = env[name()];
    for (auto *inst: this->getInst()->getPred()) {
      auto inSet = env[nameOut(inst->getID())];
      solution.insert(inSet.begin(), inSet.end());
    }
    env[name()] = solution;

    return env[name()] != oldEnv;
  }
}// namespace dcc

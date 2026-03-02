#pragma once
#include "Assemble.h"
#include "json.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
namespace dcc {
  using nlohmann::json;
  Env line2Env(std::string line) {
    json j = json::parse(line);
    std::unordered_map<std::string, int> inits_from_int = j.get<std::unordered_map<std::string, int>>();
    std::unordered_map<std::string, Env::Value> inits;
    for (auto [k, v]: inits_from_int) {
      Env::Value tmp = v;
      inits[k] = tmp;
    }
    auto env = dcc::Env(inits);
    return env;
  }
  const Env file2CFGAndEnv(std::vector<std::string> lines, std::vector<Inst *> &insts) {
    auto env = line2Env(lines[0]);
    auto split = [](const std::string &s) {
      std::istringstream iss(s);
      std::vector<std::string> tokens;
      std::string token;
      while (iss >> token) {
        tokens.push_back(token);
      }
      return tokens;
    };
    lines.erase(lines.begin());
    std::unordered_map<int, int> btMap;
    for (int i = 0; i < lines.size(); ++i) {
      auto singleInst = split(lines[i]);
      if (std::find(singleInst.begin(), singleInst.end(), "bt") != singleInst.end()) {
        auto cond = singleInst[1];
        btMap[i] = std::stoi(singleInst[2]);
        auto newInst = new Bt(cond);
        insts.push_back(newInst);
        continue;
      }
      auto dst = singleInst[0];
      auto operand1 = singleInst[3];
      auto operand2 = singleInst[4];
      if (std::find(singleInst.begin(), singleInst.end(), "add") != singleInst.end()) {
        auto newInst = new Add(dst, operand1, operand2);
        insts.push_back(newInst);
      }
      if (std::find(singleInst.begin(), singleInst.end(), "mul") != singleInst.end()) {
        auto newInst = new Mul(dst, operand1, operand2);
        insts.push_back(newInst);
      }
      if (std::find(singleInst.begin(), singleInst.end(), "lth") != singleInst.end()) {
        auto newInst = new Lth(dst, operand1, operand2);
        insts.push_back(newInst);
      }
      if (std::find(singleInst.begin(), singleInst.end(), "geq") != singleInst.end()) {
        auto newInst = new Geq(dst, operand1, operand2);
        insts.push_back(newInst);
      }
    }

    for (int i = 0; i < insts.size() - 1; ++i) {
      if (btMap.find(i) == btMap.end()) {
        insts[i]->addNext(insts[i + 1]);
      }
    }
    for (auto [i, trueIdx]: btMap) {
      auto trueInst = insts[trueIdx];
      auto falseInst = i + 1 < insts.size() ? insts[i + 1] : nullptr;
      if (auto btInst = dynamic_cast<Bt *>(insts[i])) {
        btInst->addFalseNext(falseInst);
        btInst->addTrueNext(trueInst);
      }
    }
    return env;
  }
}// namespace dcc

#include "Assemble.h"
#include "DataFlow.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
void test_min(int n, int m) {
  auto env = dcc::Env({{"m", m}, {"n", n}, {"zero", 0}});
  auto m_min = dcc::Add("answer", "m", "zero");
  auto n_min = dcc::Add("answer", "n", "zero");
  auto p = dcc::Lth("p", "n", "m");
  auto b = dcc::Bt("p", &n_min, &m_min);
  p.addNext(&b);
  dcc::interpret(&p, env);
  auto ans = env.getVar("answer");
  printf("ans=%d\n", ans);
}
static void dumpDfEnv(const dcc::DataFlowEnv &dfEnv) {
  std::stringstream ss;
  for (auto [k, v]: dfEnv) {
    ss << k << ": ";
    for (auto i: v) {
      ss << "(" << i.first << ", " << std::to_string(i.second) << ") ";
    }
    ss << "\n";
  }
  std::cout << ss.str();
}
int main(int argc, char *argv[]) {
  int m = 1, n = 2;
  // test_min(n, m);
  std::ifstream file("/home/buyi/code/catlang/dcc888Optimizer/fib.txt");
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }
  std::vector<dcc::Inst *> insts;
  auto env = dcc::file2CFGAndEnv(lines, insts);
  auto equations = dcc::genReachingDefConstraints(insts);
  // for (auto *eq: equations) {
  //   eq->dump();
  //   std::cout << "\n";
  // }
  dcc::DataFlowEnv chaosEnv = dcc::solveEquations(equations);
  // dumpDfEnv(chaosEnv);
  dcc::DataFlowEnv wlEnv = dcc::solveEquationsWithWorkList(equations);
  dumpDfEnv(wlEnv);
  // dcc::interpret(insts[0], env);
  // env.dump();

  return 0;
}

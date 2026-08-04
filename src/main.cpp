#include "../include/simulator.hpp"
#include <iostream>

int main() {
  Simulator sim;
  sim.load_program();

  // 开发阶段加一个周期上限，防止未完成的阶段导致无限循环
  const int MAX_CYCLE = 1000000;
  while (!sim.is_halted() && sim.get_cycle_count() < MAX_CYCLE) {
    sim.tick();
  }

  std::cout << sim.get_result() << std::endl;
  std::cerr << sim.get_cycle_count() << std::endl;
  return 0;
}

#include "../include/simulator.hpp"
#include <iostream>

int main() {
  Simulator sim;
  sim.load_program();

  while (!sim.is_halted()) {
    sim.tick();
  }

  std::cout << sim.get_result() << std::endl;
  // std::cerr << sim.get_cycle_count() << std::endl;
  return 0;
}

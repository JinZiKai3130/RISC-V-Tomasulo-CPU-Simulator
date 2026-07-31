#pragma once
#include "memory.hpp"
#include <cstdint>
class Simulator {
private:
  uint32_t regs[32];
  Memory memory;

  uint32_t pc;

  bool halted;
  int cnt;

public:
  Simulator();

  void load_program();
};
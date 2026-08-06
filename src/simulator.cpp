#include "../include/simulator.hpp"
#include <cstring>
#include <iostream>
#include <sstream>

Simulator::Simulator()
    : cur_reg{}, next_reg{}, cur_pc(0), next_pc(0), cur_halted(false),
      next_halted(false), cycle_count(0), redirect_valid(false),
      redirect_pc(0) {}

void Simulator::load_program() {
  std::string line;
  uint32_t addr = 0;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;
    if (line[0] == '@') {
      addr = std::stoul(line.substr(1), nullptr, 16);
    } else {
      std::istringstream iss(line);
      std::string byte_str;
      while (iss >> byte_str) {
        uint8_t byte = (uint8_t)std::stoul(byte_str, nullptr, 16);
        memory.write_byte(addr++, byte);
      }
    }
  }
}
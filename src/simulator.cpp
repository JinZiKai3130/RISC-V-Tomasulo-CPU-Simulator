#include "../include/simulator.hpp"
#include <cstring>
#include <iostream>
#include <sstream>

Simulator::Simulator() : regs{}, pc(0), halted(false), cnt(0) {}

void Simulator::load_program() {
  std::string line;
  uint32_t addr = 0;
  while (std::getline(std::cin, line)) {
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
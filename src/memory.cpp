#include "../include/memory.hpp"

uint8_t Memory::read_byte(uint32_t addr) {
  auto it = data.find(addr);
  if (it == data.end())
    return 0x00;
  return it->second;
}

void Memory::write_byte(uint32_t addr, uint8_t val) {
  if (val == 0) {
    data.erase(addr);
  } else {
    data[addr] = val;
  }
}

uint32_t Memory::read_word(uint32_t addr) {
  return (uint32_t)read_byte(addr) << 0 | (uint32_t)read_byte(addr + 1) << 8 |
         (uint32_t)read_byte(addr + 2) << 16 |
         (uint32_t)read_byte(addr + 3) << 24;
}

void Memory::write_word(uint32_t addr, uint32_t val) {
  write_byte(addr, (val >> 0) & 0xFF);
  write_byte(addr + 1, (val >> 8) & 0xFF);
  write_byte(addr + 2, (val >> 16) & 0xFF);
  write_byte(addr + 3, (val >> 24) & 0xFF);
}
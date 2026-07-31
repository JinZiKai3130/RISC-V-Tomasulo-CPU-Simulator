#pragma once
#include <cstdint>
#include <map>

class Memory {
private:
  std::map<uint32_t, uint8_t> data;

public:
  uint8_t read_byte(uint32_t);

  void write_byte(uint32_t, uint8_t);

  uint32_t read_word(uint32_t);

  void write_word(uint32_t, uint32_t);
};
#include <cstdint>
#include <iostream>
#include <map>

class Memory {
private:
  std::map<uint32_t, uint8_t> data;

public:
  uint8_t read_byte(uint32_t addr) {
    auto it = data.find(addr);
    if (it == data.end())
      return 0x00;
    return it->second;
  }

  void write_byte(uint32_t addr, uint8_t val) {
    if (val == 0) {
      data.erase(addr);
    } else {
      data[addr] = val;
    }
  }

  uint32_t read_word(uint32_t addr) {
    return (uint32_t)read_byte(addr) << 0 | (uint32_t)read_byte(addr + 1) << 8 |
           (uint32_t)read_byte(addr + 2) << 16 |
           (uint32_t)read_byte(addr + 3) << 24;
  }

  void write_word(uint32_t addr, uint32_t val) {
    write_byte(addr, (val >> 0) & 0xFF);
    write_byte(addr + 1, (val >> 8) & 0xFF);
    write_byte(addr + 2, (val >> 16) & 0xFF);
    write_byte(addr + 3, (val >> 24) & 0xFF);
  }
};

struct DecodedInst {
  uint32_t opcode;
  uint32_t rd;
  uint32_t rs1;
  uint32_t rs2;
  uint32_t funct3;
  uint32_t funct7;
  int32_t imm = 0;
  uint32_t type;
};

DecodedInst decode(uint32_t inst) {
  DecodedInst dec;
  dec.opcode = inst & 0x7f;
  dec.rd = (inst >> 7) & 0x1f;
  dec.rs1 = (inst >> 15) & 0x1f;
  dec.rs2 = (inst >> 20) & 0x1f;
  dec.funct3 = (inst >> 12) & 0x7;
  dec.funct7 = (inst >> 25) & 0x7f;
  if (dec.opcode == 0x13 && (dec.funct3 == 0x1 || dec.funct3 == 0x5)) {
    // I*
    dec.imm = dec.rs2;
  } else if (dec.opcode == 0x13 || dec.opcode == 0x3 || dec.opcode == 0x67 ||
             dec.opcode == 0x73) {
    // I
    dec.imm = dec.rs2 | (dec.funct7 << 5);
    if (dec.imm & (1 << 11)) {
      dec.imm |= 0xfffffc00;
    }
  } else if (dec.opcode == 0x23) {
    // S
    dec.imm = dec.rd | (dec.funct7 << 5);
    if (dec.imm & (1 << 11)) {
      dec.imm |= 0xfffffc00;
    }
  } else if (dec.opcode == 0x63) {
    // B
    dec.imm = (dec.rd >> 1) | (dec.funct7 >> 5) & 0x3f |
              ((dec.rd & 0x1) << 11) | ((dec.funct7 & 0x40) << 12);
    if (dec.imm & (1 << 12)) {
      dec.imm |= 0xfffff800;
    }
  } else if (dec.opcode == 0x6f) {
    // J
    dec.imm = ((dec.funct7 >> 6) & 0x1) << 20 | (dec.rs1 << 15) |
              (dec.funct3 << 12) | (dec.rs2 & 0x1) << 11 |
              (dec.funct7 & 0x3F) << 5 | ((dec.rs2 >> 1) & 0xF) << 1;
    if (dec.imm & (1 << 20))
      dec.imm |= 0xFFF00000;
  } else if (dec.opcode == 0x17 || dec.opcode == 0x37) {
    // U
    dec.imm = inst & 0xFFFFF000;
  }
  return dec;
}

void execute(DecodedInst &dec, uint32_t &pc, uint32_t regs[32], Memory &mem) {
  uint32_t cur_pc = pc;

  switch (dec.opcode) {
  case 0x13: // I和I*
    regs[dec.rd] = regs[dec.rs1] + dec.imm;
    pc = cur_pc + 4;
    break;
  case 0x33: // R
    regs[dec.rd] = regs[dec.rs1] + regs[dec.rs2];
    pc = cur_pc + 4;
    break;
  case 0x6f: // J
    regs[dec.rd] = cur_pc + 4;
    pc = cur_pc + dec.imm;
    break;
  case 0x03: // I主要是(Load Word)部分
    regs[dec.rd] = mem.read_word(regs[dec.rs1] + dec.imm);
    pc = cur_pc + 4;
    break;
  case 0x23: // S
    mem.write_word(regs[dec.rs1] + dec.imm, regs[dec.rs2]);
    pc = cur_pc + 4;
    break;
  case 0x63: // B
    break;
  case 0x67: // I jalr
    break;
  case 0x17: // U
    break;
  case 0x37: // U
    break;
  case 0x73: // I ebreak ecall
    break;
  }

  // 关键一步：无论如何，x0 永远为 0（防止之前的指令误写）
  regs[0] = 0;
}

class Simulator {
private:
  uint32_t regs[32];
  Memory memory;

  uint32_t pc;

  bool halted;

public:
  void tick() {
    uint32_t inst = memory.read_word(pc);
    DecodedInst dec = decode(inst);
    execute(dec, pc, regs, memory);
  }
};

int main() { return 0; }
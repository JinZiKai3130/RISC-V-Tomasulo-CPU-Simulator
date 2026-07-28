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

void execute(DecodedInst &dec, uint32_t &pc, uint32_t regs[32], Memory &mem,
             bool &halted) {
  uint32_t cur_pc = pc;
  pc = cur_pc + 4;

  switch (dec.opcode) {
  case 0x13: // I和I*
    switch (dec.funct3) {
    case 0x0:
      regs[dec.rd] = regs[dec.rs1] + dec.imm;
      break;

    case 0x1:
      regs[dec.rd] = regs[dec.rs1] << (dec.imm & 0x1F);
      break;

    case 0x2:
      regs[dec.rd] = ((int32_t)regs[dec.rs1] < dec.imm) ? 1 : 0;
      break;

    case 0x3:
      regs[dec.rd] = (regs[dec.rs1] < (uint32_t)dec.imm) ? 1 : 0;
      break;

    case 0x4:
      regs[dec.rd] = regs[dec.rs1] ^ dec.imm;
      break;

    case 0x5:
      if (dec.funct7 == 0x00)
        regs[dec.rd] = regs[dec.rs1] >> (dec.imm & 0x1F);
      else if (dec.funct7 == 0x20)
        regs[dec.rd] = (int32_t)regs[dec.rs1] >> (dec.imm & 0x1F);
      break;

    case 0x6:
      regs[dec.rd] = regs[dec.rs1] | dec.imm;
      break;

    case 0x7:
      regs[dec.rd] = regs[dec.rs1] & dec.imm;
      break;
    }
    break;

  case 0x33: // R
    switch (dec.funct3) {
    case 0x0:
      if (dec.funct7 == 0x00) {
        regs[dec.rd] = regs[dec.rs1] + regs[dec.rs2];
      } else {
        regs[dec.rd] = regs[dec.rs1] + (~regs[dec.rs2] + 1);
      }
      break;

    case 0x1:
      regs[dec.rd] = regs[dec.rs1] << (regs[dec.rs2] & 0x1F);
      break;

    case 0x2:
      regs[dec.rd] = ((int32_t)regs[dec.rs1] < (int32_t)regs[dec.rs2]) ? 1 : 0;
      break;

    case 0x3:
      regs[dec.rd] = (regs[dec.rs1] < regs[dec.rs2]) ? 1 : 0;
      break;

    case 0x4:
      regs[dec.rd] = regs[dec.rs1] ^ regs[dec.rs2];
      break;

    case 0x5:
      if (dec.funct7 == 0x00)
        regs[dec.rd] = regs[dec.rs1] >> (regs[dec.rs2] & 0x1F);
      else if (dec.funct7 == 0x20)
        regs[dec.rd] = (int32_t)regs[dec.rs1] >> (regs[dec.rs2] & 0x1F);
      break;

    case 0x6:
      regs[dec.rd] = regs[dec.rs1] | regs[dec.rs2];
      break;

    case 0x7:
      regs[dec.rd] = regs[dec.rs1] & regs[dec.rs2];
      break;
    }
    break;

  case 0x6f: // J
    regs[dec.rd] = cur_pc + 4;
    pc = cur_pc + dec.imm;
    break;
  case 0x03: { // I主要是Load Word部分
    uint32_t addr = regs[dec.rs1] + dec.imm;
    switch (dec.funct3) {
    case 0x0: // 带符号1byte
      regs[dec.rd] = (int32_t)(int8_t)mem.read_byte(addr);
      break;
    case 0x1: // 带符号2byte
      regs[dec.rd] = (int32_t)(int16_t)(mem.read_byte(addr) |
                                        (mem.read_byte(addr + 1) << 8));
      break;
    case 0x2: // 这里是整个可以直接4byte读出
      regs[dec.rd] = mem.read_word(addr);
      break;
    case 0x4: // 无符号1byte
      regs[dec.rd] = mem.read_byte(addr);
      break;
    case 0x5: // 无符号2byte
      regs[dec.rd] = mem.read_byte(addr) | (mem.read_byte(addr + 1) << 8);
      break;
    }
    break;
  }
  case 0x23: { // S：主要关于Store
    uint32_t addr = regs[dec.rs1] + dec.imm;
    switch (dec.funct3) {
    case 0x0:
      mem.write_byte(addr, regs[dec.rs2] & 0xFF);
      break;
    case 0x1:
      mem.write_byte(addr, regs[dec.rs2] & 0xFF);
      mem.write_byte(addr + 1, (regs[dec.rs2] >> 8) & 0xFF);
      break;
    case 0x2:
      mem.write_word(addr, regs[dec.rs2]);
      break;
    }
    break;
  }

  case 0x63: { // B 条件跳转
    bool taken = false;
    switch (dec.funct3) {
    case 0x0:
      taken = (regs[dec.rs1] == regs[dec.rs2]);
      break;
    case 0x1:
      taken = (regs[dec.rs1] != regs[dec.rs2]);
      break;
    case 0x4:
      taken = ((int32_t)regs[dec.rs1] < (int32_t)regs[dec.rs2]);
      break;
    case 0x5:
      taken = ((int32_t)regs[dec.rs1] >= (int32_t)regs[dec.rs2]);
      break;
    case 0x6:
      taken = (regs[dec.rs1] < regs[dec.rs2]);
      break;
    case 0x7:
      taken = (regs[dec.rs1] >= regs[dec.rs2]);
      break;
    }
    if (taken)
      pc = cur_pc + dec.imm;
    break;
  }
  case 0x67: // I jalr
    regs[dec.rd] = cur_pc + 4;
    pc = regs[dec.rs1] + dec.imm;
    break;
  case 0x17: // U
    regs[dec.rd] = cur_pc + dec.imm;
    break;
  case 0x37: // U
    regs[dec.rd] = dec.imm;
    break;
  case 0x73:
    if (dec.imm == 0x0)
      halted = true;
    else if (dec.imm == 0x1)
      halted = true;
    break;
  }

  regs[0] = 0;
}

class Simulator {
private:
  uint32_t regs[32];
  Memory memory;

  uint32_t pc;

  bool halted;

public:
  Simulator() : regs{}, pc(0), halted(false) {}
  void tick() {
    uint32_t inst = memory.read_word(pc);
    DecodedInst dec = decode(inst);
    execute(dec, pc, regs, memory, halted);
  }

  bool is_halted() { return halted; }
};

int main() {
  Simulator sim;

  while (!sim.is_halted()) {
    sim.tick();
  }
  return 0;
}
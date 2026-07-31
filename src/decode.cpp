#include "../include/decode.hpp"

DecodedInst::DecodedInst()
    : opcode(0), rd(0), rs1(0), rs2(0), funct3(0), funct7(0), imm(0), type(0) {}

DecodedInst::DecodedInst(uint32_t inst) {
  this->opcode = inst & 0x7f;
  this->rd = (inst >> 7) & 0x1f;
  this->rs1 = (inst >> 15) & 0x1f;
  this->rs2 = (inst >> 20) & 0x1f;
  this->funct3 = (inst >> 12) & 0x7;
  this->funct7 = (inst >> 25) & 0x7f;
  if (this->opcode == 0x13 && (this->funct3 == 0x1 || this->funct3 == 0x5)) {
    // I*
    this->imm = this->rs2;
  } else if (this->opcode == 0x13 || this->opcode == 0x3 ||
             this->opcode == 0x67 || this->opcode == 0x73) {
    // I
    this->imm = this->rs2 | (this->funct7 << 5);
    if (this->imm & (1 << 11)) {
      this->imm |= 0xfffff800;
    }
  } else if (this->opcode == 0x23) {
    // S
    this->imm = this->rd | (this->funct7 << 5);
    if (this->imm & (1 << 11)) {
      this->imm |= 0xfffff800;
    }
  } else if (this->opcode == 0x63) {
    // B
    this->imm = (this->rd & 0x1e) | ((this->funct7 & 0x3f) << 5) |
                ((this->rd & 0x1) << 11) | ((this->funct7 & 0x40) << 6);
    if (this->imm & (1 << 12)) {
      this->imm |= 0xfffff000;
    }
  } else if (this->opcode == 0x6f) {
    // J
    this->imm = ((this->funct7 >> 6) & 0x1) << 20 | (this->rs1 << 15) |
                (this->funct3 << 12) | (this->rs2 & 0x1) << 11 |
                (this->funct7 & 0x3F) << 5 | ((this->rs2 >> 1) & 0xF) << 1;
    if (this->imm & (1 << 20))
      this->imm |= 0xFFF00000;
  } else if (this->opcode == 0x17 || this->opcode == 0x37) {
    // U
    this->imm = inst & 0xFFFFF000;
  }
}
#pragma once

class RegisterStatusTable {
private:
  static const int NUM_REGS = 32; // RV32I 共 32 个寄存器
  int cur_table[NUM_REGS];
  int next_table[NUM_REGS];

public:
  RegisterStatusTable() {
    for (int i = 0; i < NUM_REGS; i++) {
      cur_table[i] = -1;
      next_table[i] = -1;
    }
  }

  void take_snapshot() {
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = cur_table[i];
    }
  }

  void update() {
    for (int i = 0; i < NUM_REGS; i++) {
      cur_table[i] = next_table[i];
    }
  }

  int get_producer(int reg) const {
    // 查找
    if (reg == 0)
      return -1;
    return cur_table[reg];
  }

  void set_producer(int reg, int rob_tag) {
    // issue来更新
    if (reg == 0)
      return; // x0 不可写
    next_table[reg] = rob_tag;
  }

  void clear_if_match(int rob_tag) {
    // CDB更新
    for (int i = 0; i < NUM_REGS; i++) {
      if (cur_table[i] == rob_tag) {
        next_table[i] = -1;
      }
    }
  }

  void flush() {
    // 这个table在flush的时候一定全部清空，只有在队头的时候才会将分支commit，然后进行适当的flush
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = -1;
    }
  }
};
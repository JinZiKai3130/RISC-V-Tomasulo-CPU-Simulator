#pragma once

class RegisterStatusTable {
private:
  inline static const int NUM_REGS = 32;
  int cur_table[NUM_REGS];
  int next_table[NUM_REGS];
  bool issue_write[NUM_REGS];  // 确定这个周期每个寄存器上是否出现了issue操作
  bool commit_write[NUM_REGS]; // 确定这个周期每个寄存器上是否出现了commit操作
  // 显然如果同一个周期内issue+commit同时出现，显然应该置为issue的值而非置为-1
public:
  RegisterStatusTable() {
    for (int i = 0; i < NUM_REGS; i++) {
      cur_table[i] = -1;
      next_table[i] = -1;
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }

  void take_snapshot() {
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = cur_table[i];
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }

  void update() {
    for (int i = 0; i < NUM_REGS; i++) {
      if (issue_write[i]) {
      } else if (commit_write[i]) {
        next_table[i] = -1;
      }
      cur_table[i] = next_table[i];
    }
  }

  int get_producer(int reg) const {
    if (reg == 0)
      return -1;
    return cur_table[reg];
  }

  void set_producer(int reg, int rob_tag) {
    if (reg == 0)
      return;
    next_table[reg] = rob_tag;
    issue_write[reg] = true;
  }

  void clear_if_match(int rob_tag) {
    for (int i = 0; i < NUM_REGS; i++) {
      if (cur_table[i] == rob_tag) {
        commit_write[i] = true;
      }
    }
  }

  void flush() {
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = -1;
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }
};
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
    // CDB更新：某指令提交时清除指向它的 RAT 条目。
    // 注意：判断必须基于 next_table（本周期快照，已包含 do_issue 本周期
    // 新写入的重命名），而不能用 cur_table。否则若本周期发射的指令刚把
    // 某寄存器重命名到新 tag，而旧 producer 又在本周期提交，
    // clear_if_match 用 cur_table（旧值）判断会误命中，把 next_table 里
    // 刚写好的新映射覆盖成 -1，导致后继指令读到陈旧寄存器值。
    for (int i = 0; i < NUM_REGS; i++) {
      if (next_table[i] == rob_tag) {
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
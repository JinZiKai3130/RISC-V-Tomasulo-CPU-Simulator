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
  RegisterStatusTable();

  void take_snapshot();

  void update();

  int get_producer(int reg) const;

  void set_producer(int reg, int rob_tag);

  void clear_if_match(int rob_tag);

  void flush();
};
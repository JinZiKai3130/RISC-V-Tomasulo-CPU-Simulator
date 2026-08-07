#pragma once
#include <cstdint>

struct ROBEntry {
  bool busy;
  int op;
  int dest_reg;
  uint32_t value; // 这里表示计算的结果，包括ALU的计算结果和load出来的数值
  bool ready;     // 判断是否算完，等待commit，变化来自write_back操作
  uint32_t pc;
  bool is_branch_taken;

  bool pred_taken;
  uint32_t pred_target;
  bool actual_taken;
  uint32_t actual_target;
  ROBEntry();
};
class ROB {
  static const int ROB_SIZE = 8;

  ROBEntry cur_rob[ROB_SIZE];
  int cur_head;
  int cur_tail;
  int cur_count;

  ROBEntry next_rob[ROB_SIZE];
  int next_issued;
  int next_committed;

  int advance(int idx) const;
  int advance_n(int idx, int n) const;

public:
  ROB();

  void take_snapshot();

  void update();

  int allocate(uint8_t op_type, int dest_reg, uint32_t pc);

  void writeback(int rob_tag, uint32_t value);

  void set_branch_actual(int rob_tag, bool taken, uint32_t target);

  void set_prediction(int rob_tag, bool taken, uint32_t target);

  bool is_head_ready() const;

  ROBEntry *get_head_entry();

  void commit_head();

  void flush();

  bool is_full() const;
  bool is_empty() const;

  bool is_ready(int rob_tag) const;
  uint32_t get_value(int rob_tag) const;
  int get_head_tag() const;
};
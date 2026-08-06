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
  ROBEntry()
      : busy(0), op(0), dest_reg(0), value(0), ready(0), pc(0),
        is_branch_taken(0), pred_taken(0), pred_target(0), actual_taken(0),
        actual_target(0) {}
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

  int advance(int idx) const { return (idx + 1) % ROB_SIZE; }
  int advance_n(int idx, int n) const { return (idx + n) % ROB_SIZE; }

public:
  ROB()
      : cur_head(0), cur_tail(0), cur_count(0), next_issued(0),
        next_committed(0) {}

  void take_snapshot() {
    for (int i = 0; i < ROB_SIZE; i++) {
      next_rob[i] = cur_rob[i];
    }
    next_issued = 0;
    next_committed = 0;
  }

  void update() {
    for (int i = 0; i < ROB_SIZE; i++) {
      cur_rob[i] = next_rob[i];
    }
    cur_head = advance_n(cur_head, next_committed);
    cur_tail = advance_n(cur_tail, next_issued);
    cur_count = cur_count + next_issued - next_committed;
  }

  int allocate(uint8_t op_type, int dest_reg, uint32_t pc);

  void writeback(int rob_tag, uint32_t value);

  void set_branch_actual(int rob_tag, bool taken, uint32_t target);

  void set_prediction(int rob_tag, bool taken, uint32_t target);

  bool is_head_ready() const;

  ROBEntry *get_head_entry();

  void commit_head();

  void flush();

  bool is_full() const { return cur_count == ROB_SIZE; }
  bool is_empty() const { return cur_count == 0; }

  bool is_ready(int rob_tag) const { return cur_rob[rob_tag].ready; }
  uint32_t get_value(int rob_tag) const { return cur_rob[rob_tag].value; }
  int get_head_tag() const { return cur_head; }
};
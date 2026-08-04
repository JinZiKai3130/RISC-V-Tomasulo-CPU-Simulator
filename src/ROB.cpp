#include "../include/ROB.hpp"

int ROB::allocate(uint8_t op_type, int dest_reg, uint32_t pc) {
  int slot = cur_tail;
  next_rob[slot].op = op_type;
  next_rob[slot].busy = 1;
  next_rob[slot].dest_reg = dest_reg;
  next_rob[slot].pc = pc;
  // 复用槽位时清空遗留状态
  next_rob[slot].value = 0;
  next_rob[slot].ready = 0;
  next_rob[slot].is_branch_taken = 0;
  next_issued++;
  return slot;
}

void ROB::writeback(int rob_tag, uint32_t value) {
  next_rob[rob_tag].value = value;
  next_rob[rob_tag].ready = 1;
}

bool ROB::is_head_ready() const { return cur_rob[cur_head].ready; }

ROBEntry *ROB::get_head_entry() { return &cur_rob[cur_head]; }

void ROB::commit_head() {
  next_rob[cur_head] = ROBEntry(); // 释放队首槽位
  next_committed++;
}

void ROB::flush(int rob_tag) {
  // TODO(分支预测)：只清空 rob_tag 之后（更年轻）的条目，保留更老的。
  // 当前先整体清空，作为占位。
  (void)rob_tag;
  for (int i = 0; i < ROB_SIZE; i++) {
    next_rob[i] = ROBEntry();
  }
  next_issued = 0;
  next_committed = cur_count;
}
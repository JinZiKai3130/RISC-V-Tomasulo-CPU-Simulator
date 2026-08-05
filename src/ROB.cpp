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
  next_rob[slot].pred_taken = 0;
  next_rob[slot].pred_target = 0;
  next_rob[slot].actual_taken = 0;
  next_rob[slot].actual_target = 0;
  next_issued++;
  return slot;
}

void ROB::writeback(int rob_tag, uint32_t value) {
  next_rob[rob_tag].value = value;
  next_rob[rob_tag].ready = 1;
}

void ROB::set_branch_actual(int rob_tag, bool taken, uint32_t target) {
  next_rob[rob_tag].actual_taken = taken;
  next_rob[rob_tag].actual_target = target;
}

void ROB::set_prediction(int rob_tag, bool taken, uint32_t target) {
  next_rob[rob_tag].pred_taken = taken;
  next_rob[rob_tag].pred_target = target;
}

bool ROB::is_head_ready() const { return cur_rob[cur_head].ready; }

ROBEntry *ROB::get_head_entry() { return &cur_rob[cur_head]; }

void ROB::commit_head() {
  next_rob[cur_head] = ROBEntry(); // 释放队首槽位
  next_committed++;
}

void ROB::flush() {
  // 分支预测错误：整体清空所有未提交条目（已提交的早已从 ROB 移除）。
  // 用 next_committed = cur_count 让 update 时 head 前进整个队列长度，
  // 恰好归位到 tail，count 归零。
  for (int i = 0; i < ROB_SIZE; i++) {
    next_rob[i] = ROBEntry();
  }
  next_issued = 0;
  next_committed = cur_count;
}
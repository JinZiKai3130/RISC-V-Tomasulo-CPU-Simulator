#include "../include/ROB.hpp"

int ROB::allocate(uint8_t op_type, int dest_reg, uint32_t pc) {
  int slot = cur_tail;
  next_rob[slot].op = op_type;
  next_rob[slot].busy = 1;
  next_rob[slot].dest_reg = dest_reg;
  next_rob[slot].pc = pc;
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
  next_rob[cur_head] = ROBEntry();
  next_committed++;
}

void ROB::flush() {
  for (int i = 0; i < ROB_SIZE; i++) {
    next_rob[i] = ROBEntry();
  }
  next_issued = 0;
  next_committed = cur_count;
}
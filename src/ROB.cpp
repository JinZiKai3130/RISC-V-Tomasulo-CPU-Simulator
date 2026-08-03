#include "../include/ROB.hpp"

int ROB::allocate(uint8_t op_type, int dest_reg, uint32_t pc, bool pred_taken) {
  rob[tail].op = op_type;
  rob[tail].busy = 1;
  rob[tail].dest_reg = dest_reg;
  rob[tail].pc = pc;
  rob[tail].is_branch_taken = pred_taken;
  count++;
  int tmp = tail;
  tail = advance(tail);

  return tmp;
}

void ROB::writeback(int rob_tag, uint32_t value) {
  rob[rob_tag].value = value;
  rob[rob_tag].ready = 1;
}

bool ROB::is_head_ready() const { return rob[head].ready; }

// ROBEntry *get_head_entry() {}

void ROB::flush(int rob_tag) {
  for (int i = 0; i < ROB_SIZE; i++) {
    rob[i] = ROBEntry();
  }
}
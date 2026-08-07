#include "../include/RAT.hpp"

RegisterStatusTable::RegisterStatusTable() {
  for (int i = 0; i < NUM_REGS; i++) {
    cur_table[i] = -1;
    next_table[i] = -1;
    issue_write[i] = false;
    commit_write[i] = false;
  }
}

void RegisterStatusTable::take_snapshot() {
  for (int i = 0; i < NUM_REGS; i++) {
    next_table[i] = cur_table[i];
    issue_write[i] = false;
    commit_write[i] = false;
  }
}

void RegisterStatusTable::update() {
  for (int i = 0; i < NUM_REGS; i++) {
    if (issue_write[i]) {
    } else if (commit_write[i]) {
      next_table[i] = -1;
    }
    cur_table[i] = next_table[i];
  }
}

int RegisterStatusTable::get_producer(int reg) const {
  if (reg == 0)
    return -1;
  return cur_table[reg];
}

void RegisterStatusTable::set_producer(int reg, int rob_tag) {
  if (reg == 0)
    return;
  next_table[reg] = rob_tag;
  issue_write[reg] = true;
}

void RegisterStatusTable::clear_if_match(int rob_tag) {
  for (int i = 0; i < NUM_REGS; i++) {
    if (cur_table[i] == rob_tag) {
      commit_write[i] = true;
    }
  }
}

void RegisterStatusTable::flush() {
  for (int i = 0; i < NUM_REGS; i++) {
    next_table[i] = -1;
    issue_write[i] = false;
    commit_write[i] = false;
  }
}

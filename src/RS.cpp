#include "../include/RS.hpp"

int ReservationStation::find_free_slot() {
  for (int i = 0; i < 8; i++) {
    if (!cur_rs[i].busy) {
      return i;
    }
  }
  return -1;
}

void ReservationStation::add(int op, int rob_tag, int vj, int vk, int qj,
                             int qk) {
  int cur_station = find_free_slot();
  auto &modified = next_rs[cur_station];
  modified.busy = 1;
  modified.op = op;
  modified.rob_tag = rob_tag;
  modified.qj = qj;
  modified.qk = qk;
  modified.vj = vj;
  modified.vk = vk;
}

void ReservationStation::wakeup(int producer_tag, int result_value) {
  for (int i = 0; i < 8; i++) {
    if (!cur_rs[i].busy)
      continue;
    if (producer_tag == cur_rs[i].qj) {
      next_rs[i].vj = result_value;
      next_rs[i].qj = -1;
    } else if (producer_tag == cur_rs[i].qk) {
      next_rs[i].vk = result_value;
      next_rs[i].qk = -1;
    }
  }
}

int ReservationStation::select_ready(int &op, int &vj, int &vk, int &rob_tag) {
  // 找到接下来的操作对象（可以用于ALU）
  for (int i = 0; i < 8; i++) {
    if (!cur_rs[i].busy)
      continue;
    if (cur_rs[i].qj == -1 && cur_rs[i].qk == -1 && !cur_rs[i].waiting) {
      op = cur_rs[i].op;
      vj = cur_rs[i].vj;
      vk = cur_rs[i].vk;
      rob_tag = cur_rs[i].rob_tag;
      return i;
    }
  }
  return -1;
}

void ReservationStation::flush() {
  for (int i = 0; i < 8; i++) {
    next_rs[i].busy = 0;
    next_rs[i].op = 0;
    next_rs[i].qj = 0;
    next_rs[i].qk = 0;
    next_rs[i].rob_tag = 0;
    next_rs[i].vj = 0;
    next_rs[i].vk = 0;
  }
}
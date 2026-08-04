#include "../include/RS.hpp"

int ReservationStation::find_free_slot() {
  for (int i = 0; i < RS_SIZE; i++) {
    if (!cur_rs[i].busy) {
      return i;
    }
  }
  return -1;
}

void ReservationStation::add(int op, int rob_tag, const DecodedInst &dec,
                             uint32_t pc, int vj, int vk, int qj, int qk) {
  int cur_station = find_free_slot();
  auto &modified = next_rs[cur_station];
  modified.busy = 1;
  modified.op = op;
  modified.opcode = dec.opcode;
  modified.funct3 = dec.funct3;
  modified.funct7 = dec.funct7;
  modified.imm = dec.imm;
  modified.pc = pc;
  modified.rob_tag = rob_tag;
  modified.qj = qj;
  modified.qk = qk;
  modified.vj = vj;
  modified.vk = vk;
  modified.waiting = 0; // 新条目默认等待执行
  modified.result = 0;
  modified.branch_taken = 0;
  modified.branch_target = 0;
}

void ReservationStation::set_result(int idx, int result) {
  next_rs[idx].result = result;
  next_rs[idx].waiting = 1;
}

void ReservationStation::set_branch(int idx, bool taken, int target,
                                    int link_result) {
  next_rs[idx].branch_taken = taken;
  next_rs[idx].branch_target = target;
  next_rs[idx].result = link_result;
  next_rs[idx].waiting = 1;
}

void ReservationStation::lock(int idx) { next_rs[idx].waiting = 1; }

void ReservationStation::wakeup(int producer_tag, int result_value) {
  // 在 next_rs 上操作：快照后 next 已包含本周期新发射的条目。
  // 必须这样——如果某指令发射的同一周期其生产者正好广播，
  // 该新条目（只在 next 中）的 qj/qk 才能被这次广播清除，避免死锁。
  for (int i = 0; i < RS_SIZE; i++) {
    if (!next_rs[i].busy)
      continue;
    // 注意：qj 与 qk 可能指向同一个生产者标签（如 add x3,x2,x2），
    // 必须用两个独立 if 而不是 else-if，否则只清一个、另一个死锁。
    if (producer_tag == next_rs[i].qj) {
      next_rs[i].vj = result_value;
      next_rs[i].qj = -1;
    }
    if (producer_tag == next_rs[i].qk) {
      next_rs[i].vk = result_value;
      next_rs[i].qk = -1;
    }
  }
}

int ReservationStation::select_ready(int &op, int &vj, int &vk, int &rob_tag) {
  // 找到接下来的操作对象（可以用于ALU）
  for (int i = 0; i < RS_SIZE; i++) {
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

int ReservationStation::select_waiting(int &rob_tag, int &result) {
  // 选一个"已算完（waiting）"的 ALU(0)/Branch(1) 条目等待 CDB 广播。
  // 注意：Load(3)/Store(2) 的结果由 LSQ 提供，不经过这里广播。
  for (int i = 0; i < RS_SIZE; i++) {
    if (!cur_rs[i].busy)
      continue;
    if (cur_rs[i].waiting && (cur_rs[i].op == 0 || cur_rs[i].op == 1)) {
      rob_tag = cur_rs[i].rob_tag;
      result = cur_rs[i].result;
      return i;
    }
  }
  return -1;
}

int ReservationStation::find_by_rob(int rob_tag) {
  for (int i = 0; i < RS_SIZE; i++) {
    if (cur_rs[i].busy && cur_rs[i].rob_tag == rob_tag)
      return i;
  }
  return -1;
}

void ReservationStation::release(int idx) {
  if (idx < 0 || idx >= RS_SIZE || !cur_rs[idx].busy)
    return;
  next_rs[idx] = RS_Entry();
}

void ReservationStation::flush() {
  for (int i = 0; i < RS_SIZE; i++) {
    next_rs[i] = RS_Entry();
  }
}
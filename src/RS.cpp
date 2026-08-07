#include "../include/RS.hpp"

RS_Entry::RS_Entry()
    : busy(0), op(0), opcode(0), funct3(0), funct7(0), imm(0), pc(0), vj(0),
      vk(0), qj(0), qk(0), rob_tag(0), waiting(0), result(0), branch_taken(0),
      branch_target(0) {}

ReservationStation::ReservationStation()
    : next_cdb_valid(false), next_cdb_tag(-1), next_cdb_value(0) {}

void ReservationStation::take_snapshot() {
  for (int i = 0; i < RS_SIZE; i++) {
    next_rs[i] = cur_rs[i];
  }
  next_cdb_valid = false;
}

void ReservationStation::update() {
  if (next_cdb_valid) {
    // 这里确定有来自cdb(writeback)更新的操作，在update的时候进行相应的更新
    for (int i = 0; i < RS_SIZE; i++) {
      if (!next_rs[i].busy)
        continue;
      if (next_cdb_tag == next_rs[i].qj) {
        next_rs[i].vj = (int)next_cdb_value;
        next_rs[i].qj = -1;
      }
      if (next_cdb_tag == next_rs[i].qk) {
        next_rs[i].vk = (int)next_cdb_value;
        next_rs[i].qk = -1;
      }
    }
  }
  for (int i = 0; i < RS_SIZE; i++) {
    cur_rs[i] = next_rs[i];
  }
}

const RS_Entry &ReservationStation::peek(int idx) const { return cur_rs[idx]; }

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
  modified.waiting = 0;
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

void ReservationStation::set_broadcast(int producer_tag,
                                       uint32_t result_value) {
  next_cdb_tag = producer_tag;
  next_cdb_value = result_value;
  next_cdb_valid = true;
}

int ReservationStation::select_ready(int &op, int &vj, int &vk, int &rob_tag) {
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
  next_cdb_valid = false;
}

bool ReservationStation::is_full() { return find_free_slot() == -1; }
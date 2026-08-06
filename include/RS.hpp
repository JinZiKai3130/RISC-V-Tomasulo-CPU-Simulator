#pragma once
#include "decode.hpp"

struct RS_Entry {
  bool busy;
  int op; // 0=ALU, 1=Branch, 2=Store, 3=Load
  uint32_t opcode;
  uint32_t funct3;
  uint32_t funct7;
  int32_t imm;
  uint32_t pc;
  int vj, vk;
  int qj, qk;
  int rob_tag;
  bool waiting;
  int result;
  bool branch_taken;
  int branch_target;
  RS_Entry()
      : busy(0), op(0), opcode(0), funct3(0), funct7(0), imm(0), pc(0), vj(0),
        vk(0), qj(0), qk(0), rob_tag(0), waiting(0), result(0), branch_taken(0),
        branch_target(0) {}
};

class ReservationStation {
  static const int RS_SIZE = 32;

  RS_Entry cur_rs[RS_SIZE];
  RS_Entry next_rs[RS_SIZE];
  bool next_cdb_valid;
  int next_cdb_tag;
  uint32_t next_cdb_value;

  int find_free_slot();

public:
  ReservationStation()
      : next_cdb_valid(false), next_cdb_tag(-1), next_cdb_value(0) {}

  void take_snapshot() {
    for (int i = 0; i < RS_SIZE; i++) {
      next_rs[i] = cur_rs[i];
    }
    next_cdb_valid = false;
  }

  void update() {
    if (next_cdb_valid) {
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

  void add(int op, int rob_tag, const DecodedInst &dec, uint32_t pc, int vj,
           int vk, int qj, int qk);

  const RS_Entry &peek(int idx) const { return cur_rs[idx]; }

  void set_result(int idx, int result);

  void set_branch(int idx, bool taken, int target, int link_result);

  void lock(int idx);

  void set_broadcast(int producer_tag, uint32_t result_value);

  int select_ready(int &op, int &vj, int &vk, int &rob_tag);

  int select_waiting(int &rob_tag, int &result);

  int find_by_rob(int rob_tag);

  void release(int idx);

  void flush();

  bool is_full() { return find_free_slot() == -1; }
};
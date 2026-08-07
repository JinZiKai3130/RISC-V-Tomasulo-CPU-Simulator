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
  RS_Entry();
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
  ReservationStation();

  void take_snapshot();

  void update();

  void add(int op, int rob_tag, const DecodedInst &dec, uint32_t pc, int vj,
           int vk, int qj, int qk);

  const RS_Entry &peek(int idx) const;

  void set_result(int idx, int result);

  void set_branch(int idx, bool taken, int target, int link_result);

  void lock(int idx);

  void set_broadcast(int producer_tag, uint32_t result_value);

  int select_ready(int &op, int &vj, int &vk, int &rob_tag);

  int select_waiting(int &rob_tag, int &result);

  int find_by_rob(int rob_tag);

  void release(int idx);

  void flush();

  bool is_full();
};
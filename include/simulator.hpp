#pragma once
#include "LSQ.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "decode.hpp"
#include "memory.hpp"
#include <cstdint>
class Simulator {
private:
  static const int REG_SIZE = 32; // 32 个寄存器
  uint32_t cur_reg[REG_SIZE];
  uint32_t next_reg[REG_SIZE];
  RegisterStatusTable status;

  ReservationStation rs;

  ROB rob;
  LoadStoreQueue lsq;

  Memory memory;
  uint32_t cur_pc, next_pc;
  bool cur_halted, next_halted;
  int cycle_count = 0;

  bool redirect_valid;
  uint32_t redirect_pc;

  enum { OP_ALU = 0, OP_BRANCH = 1, OP_STORE = 2, OP_LOAD = 3 };

  void snapshot_all();
  void update_all();
  void do_commit();
  void do_writeback();
  static int compute_alu(const RS_Entry &e, int vj, int vk);
  static bool eval_branch(const RS_Entry &e, int vj, int vk);
  void do_execute();
  void do_issue();

public:
  Simulator();

  void load_program();

  void tick();
  bool is_halted() const;
  uint32_t get_result() const;
  int get_cycle_count() const;
};
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

  void snapshot_all() {
    // 这里将当前状态全部在next里面记录一边
    for (int i = 0; i < REG_SIZE; i++) {
      next_reg[i] = cur_reg[i];
    }
    next_reg[0] = 0;
    status.take_snapshot();
    rs.take_snapshot();
    rob.take_snapshot();
    lsq.take_snapshot();
    next_pc = cur_pc;
    next_halted = cur_halted;
    redirect_valid = false;
  }

  void update_all() {
    if (redirect_valid) {
      // 如果出现了预测错误
      rob.flush();
      rs.flush();
      lsq.flush();
      status.flush();
      next_pc = redirect_pc;
      next_halted = false;
      redirect_valid = false;
    }
    for (int i = 0; i < REG_SIZE; i++) {
      cur_reg[i] = next_reg[i];
    }
    status.update();
    rs.update();
    rob.update();
    lsq.update();
    cur_pc = next_pc;
    cur_halted = next_halted;
  }

  void do_commit() {
    // 无法提交
    if (rob.is_empty() || !rob.is_head_ready())
      return;

    ROBEntry *head = rob.get_head_entry();
    int head_tag = rob.get_head_tag();

    switch (head->op) {
    case OP_ALU: {
      if (head->dest_reg != 0)
        next_reg[head->dest_reg] = head->value;
      status.clear_if_match(head_tag);
      rob.commit_head();
      break;
    }
    case OP_LOAD: {
      if (lsq.commit_head()) {
        if (head->dest_reg != 0)
          next_reg[head->dest_reg] = head->value;
        status.clear_if_match(head_tag);
        rob.commit_head();
      }
      break;
    }
    case OP_STORE: {
      // 这里lsq进入commit进程，如果commit好了就从lsq中删掉，和rob一起去掉
      lsq.start_commit_head();
      if (lsq.is_head_write_done()) {
        lsq.commit_store_head();
        rob.commit_head();
      }
      break;
    }
    case OP_BRANCH: {
      // 无预测跳转
      if (head->dest_reg > 0)
        next_reg[head->dest_reg] = head->value;
      status.clear_if_match(head_tag);

      // 预测错误
      bool mispred = (head->pred_taken != head->actual_taken) ||
                     (head->pred_target != head->actual_target);
      if (mispred) {
        redirect_valid = true;
        redirect_pc = head->actual_target;
      } else {
        // 预测正确
        rob.commit_head();
      }
      break;
    }
    default:
      rob.commit_head();
      break;
    }
  }

  void do_writeback() {
    int tag = -1;
    uint32_t value = 0;
    int lsq_idx = lsq.find_done_load();
    int rs_idx = -1;

    // 如果有lsq的load完成，则先完成lsq
    if (lsq_idx != -1) {
      tag = lsq.get_rob_index(lsq_idx);
      value = lsq.get_value(lsq_idx);
    } else {
      int result_int = 0;
      rs_idx = rs.select_waiting(tag, result_int);
      value = (uint32_t)result_int;
    }

    if (tag != -1) {
      // 这里统一不更新RS和RAT，确保不会出现两个操作冲突
      rs.set_broadcast(tag, value);
      rob.writeback(tag, value);
      lsq.set_data_by_rob(tag, value);

      if (rs_idx != -1 && rs.peek(rs_idx).op == OP_BRANCH) {
        const RS_Entry &e = rs.peek(rs_idx);
        rob.set_branch_actual(tag, e.branch_taken, (uint32_t)e.branch_target);
      }

      if (lsq_idx != -1) {
        lsq.mark_broadcasted(lsq_idx);
        rs_idx = rs.find_by_rob(tag);
      }
      if (rs_idx != -1)
        rs.release(rs_idx);
    }

    int st_idx = lsq.find_ready_store();
    if (st_idx != -1) {
      int st_tag = lsq.get_rob_index(st_idx);
      rob.writeback(st_tag, 0);
      lsq.mark_broadcasted(st_idx);
      int rsidx = rs.find_by_rob(st_tag);
      if (rsidx != -1)
        rs.release(rsidx);
    }
  }

  static int compute_alu(const RS_Entry &e, int vj, int vk) {
    uint32_t a = (uint32_t)vj;
    uint32_t b = (uint32_t)vk;
    switch (e.opcode) {
    case 0x13:
      switch (e.funct3) {
      case 0x0:
        return (int)(a + (uint32_t)e.imm); // addi
      case 0x1:
        return (int)(a << (e.imm & 0x1F)); // slli
      case 0x2:
        return ((int32_t)vj < e.imm) ? 1 : 0; // slti
      case 0x3:
        return (a < (uint32_t)e.imm) ? 1 : 0; // sltiu
      case 0x4:
        return (int)(a ^ (uint32_t)e.imm); // xori
      case 0x5:                            // slli / srai
        if (e.funct7 == 0x00)
          return (int)(a >> (e.imm & 0x1F)); // 逻辑右移
        else if (e.funct7 == 0x20)
          return (int)((int32_t)vj >> (e.imm & 0x1F)); // 算术右移
        break;
      case 0x6:
        return (int)(a | (uint32_t)e.imm); // ori
      case 0x7:
        return (int)(a & (uint32_t)e.imm); // andi
      }
      break;
    case 0x33: // R 型
      switch (e.funct3) {
      case 0x0:
        if (e.funct7 == 0x00)
          return (int)(a + b); // add
        else
          return (int)(a + (~b + 1)); // sub
      case 0x1:
        return (int)(a << (b & 0x1F)); // sll
      case 0x2:
        return ((int32_t)vj < (int32_t)vk) ? 1 : 0; // slt
      case 0x3:
        return (a < b) ? 1 : 0; // sltu
      case 0x4:
        return (int)(a ^ b); // xor
      case 0x5:              // srl / sra
        if (e.funct7 == 0x00)
          return (int)(a >> (b & 0x1F)); // 逻辑右移
        else if (e.funct7 == 0x20)
          return (int)((int32_t)vj >> (vk & 0x1F)); // 算术右移
        break;
      case 0x6:
        return (int)(a | b); // or
      case 0x7:
        return (int)(a & b); // and
      }
      break;
    case 0x17:
      return (int)(e.pc + (uint32_t)e.imm); // auipc
    case 0x37:
      return e.imm; // lui
    }
    return 0;
  }

  static bool eval_branch(const RS_Entry &e, int vj, int vk) {
    switch (e.funct3) {
    case 0x0:
      return vj == vk; // beq
    case 0x1:
      return vj != vk; // bne
    case 0x4:
      return (int32_t)vj < (int32_t)vk; // blt
    case 0x5:
      return (int32_t)vj >= (int32_t)vk; // bge
    case 0x6:
      return (uint32_t)vj < (uint32_t)vk; // bltu
    case 0x7:
      return (uint32_t)vj >= (uint32_t)vk; // bgeu
    }
    return false;
  }

  void do_execute() {
    int op, vj, vk, rob_tag;
    int idx = rs.select_ready(op, vj, vk, rob_tag);
    if (idx == -1)
      return;

    const RS_Entry &e = rs.peek(idx);
    switch (op) {
    case OP_ALU: {
      rs.set_result(idx, compute_alu(e, vj, vk));
      break;
    }
    case OP_BRANCH: {
      bool taken;
      int target;
      int link = 0;
      if (e.opcode == 0x63) {
        taken = eval_branch(e, vj, vk);
        target = taken ? (int)(e.pc + e.imm) : (int)(e.pc + 4);
      } else if (e.opcode == 0x6f) { // jal
        taken = true;
        target = (int)(e.pc + e.imm);
        link = (int)(e.pc + 4);
      } else { // 0x67 jalr
        taken = true;
        target = vj + e.imm;
        link = (int)(e.pc + 4);
      }
      rs.set_branch(idx, taken, target, link);
      break;
    }
    case OP_LOAD:
    case OP_STORE: {
      uint32_t addr = (uint32_t)(vj + e.imm);
      lsq.set_addr_by_rob(rob_tag, addr);
      if (op == OP_STORE) {
        lsq.set_data_by_rob(rob_tag, (uint32_t)vk);
      }
      rs.lock(idx);
      break;
    }
    default:
      break;
    }
  }

  void do_issue() {
    if (cur_halted)
      return;
    uint32_t inst = memory.read_word(cur_pc);
    DecodedInst dec(inst);
    if (inst == 0x0ff00513u) {
      next_halted = true;
      return;
    }

    bool is_load = (dec.opcode == 0x03);
    bool is_store = (dec.opcode == 0x23);
    bool is_branch = (dec.opcode == 0x63);
    bool is_jump = (dec.opcode == 0x6f || dec.opcode == 0x67);

    uint8_t op_type;
    if (is_branch || is_jump)
      op_type = OP_BRANCH;
    else if (is_store)
      op_type = OP_STORE;
    else if (is_load)
      op_type = OP_LOAD;
    else
      op_type = OP_ALU;

    if (rob.is_full() || rs.is_full())
      return;
    if ((is_load || is_store) && lsq.is_full())
      return;

    bool pred_taken = false;
    uint32_t pred_target = cur_pc + 4;

    bool writes_reg = !is_branch && !is_store;
    int dest_reg = writes_reg ? (int)dec.rd : -1;
    int rob_idx = rob.allocate(op_type, dest_reg, cur_pc);
    rob.set_prediction(rob_idx, pred_taken, pred_target);

    if (is_load || is_store) {
      lsq.allocate(is_store, rob_idx, dec.funct3);
    }

    bool has_rs1 = true, has_rs2 = false;
    switch (dec.opcode) {
    case 0x33:
    case 0x63:
    case 0x23:
      has_rs2 = true;
      break;
    case 0x6f:
    case 0x37:
    case 0x17:
      has_rs1 = false;
      break;
    default:
      break;
    }

    int qj = has_rs1 ? status.get_producer(dec.rs1) : -1;
    int qk = has_rs2 ? status.get_producer(dec.rs2) : -1;
    int vj = 0, vk = 0;
    if (has_rs1) {
      if (qj != -1 && rob.is_ready(qj)) {
        vj = (int)rob.get_value(qj);
        qj = -1;
      } else if (qj == -1) {
        vj = (int)cur_reg[dec.rs1];
      }
    }
    if (has_rs2) {
      if (qk != -1 && rob.is_ready(qk)) {
        vk = (int)rob.get_value(qk);
        qk = -1;
      } else if (qk == -1) {
        vk = (int)cur_reg[dec.rs2];
      }
    }

    rs.add(op_type, rob_idx, dec, cur_pc, vj, vk, qj, qk);

    if (writes_reg && dec.rd != 0) {
      status.set_producer(dec.rd, rob_idx);
    }

    next_pc = pred_target;
  }

public:
  Simulator();

  void load_program();

  void tick() {
    snapshot_all();

    do_commit();
    do_writeback();
    do_issue();

    lsq.step(memory);
    do_execute();

    update_all();

    cycle_count++;
  }

  bool is_halted() const { return cur_halted && rob.is_empty(); }

  uint32_t get_result() const { return cur_reg[10] & 0xff; }

  int get_cycle_count() const { return cycle_count; }
};
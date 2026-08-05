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
  // ---------- 所有结构都持有 cur 和 next ----------
  static const int REG_SIZE = 32; // RV32I 共 32 个寄存器
  uint32_t cur_reg[REG_SIZE];     // 内部包含 cur_regs[32], next_regs[32]
  uint32_t next_reg[REG_SIZE];
  RegisterStatusTable status; // 内部包含 cur_table[32], next_table[32]

  ReservationStation rs;

  ROB rob;            // 内部包含 cur_entries, next_entries
  LoadStoreQueue lsq; // 内部包含 cur_entries, next_entries

  Memory memory;
  uint32_t cur_pc, next_pc;
  bool cur_halted, next_halted;
  int cycle_count = 0;

  // 指令类别（与 ROB/RS 的 op 字段保持一致）
  enum { OP_ALU = 0, OP_BRANCH = 1, OP_STORE = 2, OP_LOAD = 3 };

  // ---------- 辅助函数：周期开始时快照，周期结束时交换 ----------
  void snapshot_all() {
    // 将当前的 cur 拷贝到 next，作为本周期修改的起点
    // 注意：不能用 swap，因为 swap 会丢失下一周期的初始状态
    for (int i = 0; i < REG_SIZE; i++) {
      next_reg[i] = cur_reg[i];
    }
    next_reg[0] = 0;        // x0 恒为 0
    status.take_snapshot(); // next = cur
    rs.take_snapshot();     // next = cur
    rob.take_snapshot();    // next = cur
    lsq.take_snapshot();    // next = cur
    // PC / halted 也需要快照：发射停顿（不更新 PC）时保持 next_pc = cur_pc
    next_pc = cur_pc;
    next_halted = cur_halted;
  }

  void update_all() {
    for (int i = 0; i < REG_SIZE; i++) {
      cur_reg[i] = next_reg[i];
    }
    status.update(); // cur = next
    rs.update();     // cur = next
    rob.update();    // cur = next
    lsq.update();    // cur = next
    cur_pc = next_pc;
    cur_halted = next_halted;
  }

  // ---------- 各个阶段（全部读取 cur，全部写入 next） ----------
  void do_commit() {
    // 队首未就绪 / ROB 空：无法提交
    if (rob.is_empty() || !rob.is_head_ready())
      return;

    ROBEntry *head = rob.get_head_entry();

    switch (head->op) {
    case OP_ALU: {
      // 写寄存器（ROB.value 来自 CDB 广播）
      if (head->dest_reg != 0)
        next_reg[head->dest_reg] = head->value;
      rob.commit_head();
      break;
    }
    case OP_LOAD: {
      // 写寄存器 + 释放 LSQ 队首 + 释放 ROB 队首
      if (lsq.commit_head()) {
        if (head->dest_reg != 0)
          next_reg[head->dest_reg] = head->value;
        rob.commit_head();
      }
      break;
    }
    case OP_STORE: {
      // 队首 store 提交：此刻才正式启动内存写（投机期不碰内存）。
      // 启动后由 LSQ::step() 串行写 3 周期，写完成当周期才提交 ROB。
      lsq.start_commit_head();  // 未启动则标记 committed（排队写内存）
      if (lsq.store_finished()) // 本周期写完成 → 释放 ROB 队首
        rob.commit_head();
      break;
    }
    case OP_BRANCH: {
      // 1. jal/jalr 写链接寄存器（pc+4，确定结果，与预测无关；条件分支 dest=-1
      // 跳过）
      if (head->dest_reg > 0)
        next_reg[head->dest_reg] = head->value;
      // 2. 比较预测 vs 实际：方向或目标任一不同 → 预测错误
      bool mispred = (head->pred_taken != head->actual_taken) ||
                     (head->pred_target != head->actual_target);
      if (mispred) {
        // 全部推翻重来：清空所有投机结构（ROB/RS/LSQ/RAT）。
        // 已提交的寄存器/内存结果不受影响（分支在队首时比它老的都已提交），
        // 从实际目标重新取指。
        // 注意：错误路径上可能已取到 halt（next_halted=true），它是推测的，
        // 必须一并复位——halt 不进流水线，分支之后的 halt 一定是错误路径的。
        next_halted = false;
        rob.flush();
        rs.flush();
        lsq.flush();
        status.flush();
        next_pc = head->actual_target;
      } else {
        // 预测正确：正常提交
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
    // ---- 1. CDB 广播：Load 完成优先于 ALU/Branch（仲裁）----
    int tag = -1;
    uint32_t value = 0;
    int lsq_idx = lsq.find_done_load();
    int rs_idx = -1;

    if (lsq_idx != -1) {
      tag = lsq.get_rob_index(lsq_idx);
      value = lsq.get_value(lsq_idx);
    } else {
      // ALU / 分支结果广播（值来自 RS）
      int result_int = 0;
      rs_idx = rs.select_waiting(tag, result_int);
      value = (uint32_t)result_int;
    }

    if (tag != -1) {
      // 广播副作用（全部写各部件 next）
      rs.wakeup(tag, value);      // 唤醒 RS 依赖者（清 qj/qk）
      rob.writeback(tag, value);  // 填 ROB（load/ALU 写结果；分支暂记 link 值）
      status.clear_if_match(tag); // 清 RAT
      lsq.set_data_by_rob(tag, value); // store 的数据到达

      // 分支：把实际方向/目标写入 ROB（供 commit 时比较预测）
      if (rs_idx != -1 && rs.peek(rs_idx).op == OP_BRANCH) {
        const RS_Entry &e = rs.peek(rs_idx);
        rob.set_branch_actual(tag, e.branch_taken, (uint32_t)e.branch_target);
      }

      if (lsq_idx != -1) {
        lsq.mark_broadcasted(lsq_idx);
        rs_idx = rs.find_by_rob(tag); // 释放 load 的 RS 槽位
      }
      if (rs_idx != -1)
        rs.release(rs_idx);
    }

    // ---- 2. store 地址+数据就绪 → 通知 ROB（标 ready）、释放其 RS ----
    // store 不再做投机期假访问：就绪即可通知 ROB；真正的内存写在提交
    // 启动（lsq.start_commit_head）后由 LSQ::step() 串行执行。
    int st_idx = lsq.find_ready_store();
    if (st_idx != -1) {
      int st_tag = lsq.get_rob_index(st_idx);
      rob.writeback(st_tag, 0); // store 无结果值，仅标 ready
      lsq.mark_broadcasted(st_idx);
      int rsidx = rs.find_by_rob(st_tag);
      if (rsidx != -1)
        rs.release(rsidx);
    }
  }

  // ---------- 复用 naive_simulator 中的计算逻辑 ----------
  // naive 里是 regs[rs1]/regs[rs2]，这里换成 RS 条目的 vj/vk（就绪的操作数值）
  static int compute_alu(const RS_Entry &e, int vj, int vk) {
    uint32_t a = (uint32_t)vj;
    uint32_t b = (uint32_t)vk;
    switch (e.opcode) {
    case 0x13: // I 型（含 I* 移位）
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

  // 分支条件判断（同 naive 的 0x63 分支逻辑）
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

  // ---------- 执行 ----------
  // 从 RS.cur 选一条操作数就绪、未被锁定的指令，计算结果写入 RS.next /
  // LSQ.next。 本阶段**不做 CDB 广播**：广播统一由 do_writeback
  // 阶段完成（见下方说明）。
  void do_execute() {
    // 1. 选择可执行条目（qj/qk 就绪且未被锁定）
    int op, vj, vk, rob_tag;
    int idx = rs.select_ready(op, vj, vk, rob_tag);
    if (idx == -1)
      return; // 本周期没有可执行的指令

    // 2. 读取该条目执行所需的信息（imm/pc/opcode/funct 等）
    const RS_Entry &e = rs.peek(idx);

    switch (op) {
    case OP_ALU: {
      // 算好 ALU 结果，写入 RS.next 并标记"已算完，等待 CDB 广播"
      rs.set_result(idx, compute_alu(e, vj, vk));
      break;
    }
    case OP_BRANCH: {
      // 分支/跳转：算出实际方向与目标（预测留到后续阶段处理）
      bool taken;
      int target;
      int link = 0;           // jal/jalr 写入 rd 的值（pc+4）
      if (e.opcode == 0x63) { // 条件分支
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
      // 访存：算地址填入 LSQ.next（内存访问由 LSQ 按延迟进行）
      uint32_t addr = (uint32_t)(vj + e.imm);
      lsq.set_addr_by_rob(rob_tag, addr);
      if (op == OP_STORE) {
        // select_ready 要求 qk 就绪，故 store 的数据此时一定已就绪
        lsq.set_data_by_rob(rob_tag, (uint32_t)vk);
      }
      // 锁定条目（waiting），防止再次被 select_ready 选中
      rs.lock(idx);
      break;
    }
    default:
      break;
    }
  }

  // ---------- 发射（含取指） ----------
  // 本设计没有独立的取指缓冲：每个周期在发射阶段取一条指令并尝试发射。
  // 全部读取 cur_*，全部写入 next_*。
  void do_issue() {
    // 已停机：不再取指
    if (cur_halted)
      return;

    // 1. 取指并解码
    uint32_t inst = memory.read_word(cur_pc);
    DecodedInst dec(inst);

    // 2. 终止条件：遇到 li a0, 255 (0x0ff00513) 时不执行，直接停机
    if (inst == 0x0ff00513u) {
      next_halted = true;
      return;
    }

    // 3. 判定指令类别（决定 op_type 与资源需求）
    bool is_load = (dec.opcode == 0x03);   // lb/lh/lw/lbu/lhu
    bool is_store = (dec.opcode == 0x23);  // sb/sh/sw
    bool is_branch = (dec.opcode == 0x63); // beq/bne/blt/bge/...
    bool is_jump = (dec.opcode == 0x6f || dec.opcode == 0x67); // jal / jalr

    uint8_t op_type;
    if (is_branch || is_jump)
      op_type = OP_BRANCH;
    else if (is_store)
      op_type = OP_STORE;
    else if (is_load)
      op_type = OP_LOAD;
    else
      op_type = OP_ALU; // R 型 / I 型 ALU / lui / auipc

    // 4. 资源检查：任一结构满则停顿（PC 不动，next_pc 保持 cur_pc）
    if (rob.is_full() || rs.is_full())
      return;
    if ((is_load || is_store) && lsq.is_full())
      return;

    // 5. 分支预测：统一预测"不跳转"（即顺序执行下一条 pc+4）。
    //    之后接入 Predictor 时，用预测器输出替换 pred_taken / pred_target。
    //    预测方向/目标会存入 ROB，commit 时与实际结果比较。
    bool pred_taken = false;
    uint32_t pred_target = cur_pc + 4;

    // 6. 分配 ROB 槽位（条件分支不写寄存器；jump 写 link 值；store 不写）
    bool writes_reg = !is_branch && !is_store;
    int dest_reg = writes_reg ? (int)dec.rd : -1;
    int rob_idx = rob.allocate(op_type, dest_reg, cur_pc);
    // 记录预测方向/目标，供 commit 时比较
    rob.set_prediction(rob_idx, pred_taken, pred_target);

    // 7. 分配 LSQ 槽位（仅访存指令）
    if (is_load || is_store) {
      lsq.allocate(is_store, rob_idx, dec.funct3);
    }

    // 8. 判定真正的源寄存器
    //    注意：decode 里 I/S/B/J 型指令的 rs2/rd 字段被复用为立即数的一部分，
    //    只有真正携带寄存器号的字段才能用于查状态表 / 读寄存器堆
    bool has_rs1 = true, has_rs2 = false;
    switch (dec.opcode) {
    case 0x33: // R 型：rs1 + rs2
    case 0x63: // B 型（branch）：rs1 + rs2
    case 0x23: // S 型（store）：rs1(基址) + rs2(数据)
      has_rs2 = true;
      break;
    case 0x6f: // JAL：无源寄存器
    case 0x37: // LUI
    case 0x17: // AUIPC
      has_rs1 = false;
      break;
    default: // I 型（ALU / load / jalr）：仅 rs1
      break;
    }

    // 9. 读取源操作数：RAT 中有生产者标签则置 q=tag 等待，否则直接读寄存器
    int qj = has_rs1 ? status.get_producer(dec.rs1) : -1;
    int qk = has_rs2 ? status.get_producer(dec.rs2) : -1;
    int vj = (has_rs1 && qj == -1) ? cur_reg[dec.rs1] : 0;
    int vk = (has_rs2 && qk == -1) ? cur_reg[dec.rs2] : 0;

    // 10. 分配 RS 槽位（所有指令都占用：ALU/分支算结果，访存算地址）
    //     同时拷贝执行所需的 dec/pc 信息（opcode/funct3/funct7/imm/pc）
    rs.add(op_type, rob_idx, dec, cur_pc, vj, vk, qj, qk);

    // 11. 更新 RAT：目的寄存器映射到本指令的 ROB 标签
    if (writes_reg && dec.rd != 0) {
      status.set_producer(dec.rd, rob_idx);
    }

    // 12. 更新 PC（预测目标；暂按顺序 +4）
    next_pc = pred_target;
  }

public:
  Simulator();

  void load_program();

  // 运行一个时钟周期
  void tick() {
    // 1. 快照：将当前 cur 拷贝到 next（确定本周期的输入边界）
    snapshot_all();

    // 2. 执行四个阶段（顺序可以任意打乱！）
    //    因为都读 cur、写 next，所以下面几行任意排列组合结果都一样。
    do_issue();
    do_execute();
    lsq.step(memory); // 内存单元每周期推进（读 cur 写 next）
    do_writeback();
    do_commit();

    // 3. 原子交换：所有 next 变为新的 cur，进入下一周期
    update_all();

    cycle_count++;
  }

  // 停机判定：已取到 halt 指令（cur_halted，停止取指）且流水线排空（ROB 空）。
  // 乱序执行下必须等所有指令提交完，a0 才是最终值。
  bool is_halted() const { return cur_halted && rob.is_empty(); }

  // 程序返回值：a0(x10) 的低 8 位
  uint32_t get_result() const { return cur_reg[10] & 0xff; }

  int get_cycle_count() const { return cycle_count; }
};
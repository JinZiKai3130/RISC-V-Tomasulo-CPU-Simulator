#pragma once
#include "decode.hpp"

struct RS_Entry {
  bool busy; // 只要里面还存有数据，就是busy状态
  int op;    // 指令类别：0=ALU, 1=Branch, 2=Store, 3=Load
  // ---- 执行所需信息（issue 时从 DecodedInst 拷贝）----
  uint32_t
      opcode; // 原 opcode，区分具体操作（0x13/0x33/0x63/0x6f/0x67/0x17/0x37）
  uint32_t funct3;
  uint32_t funct7;
  int32_t imm;
  uint32_t pc;       // 指令 PC（分支/跳转/auipc 需要）
  int vj, vk;        // 操作数值
  int qj, qk;        // 依赖标签（-1 表示就绪）
  int rob_tag;       // ROB 的 index
  bool waiting;      // 已算完（结果在 result 中），等待 CDB 广播；也用于锁定
  int result;        // 寄存器要写的计算结果（jal/jalr 为 pc+4）
  bool branch_taken; // 分支/跳转：实际是否跳转
  int branch_target; // 分支/跳转：实际目标 PC
  RS_Entry()
      : busy(0), op(0), opcode(0), funct3(0), funct7(0), imm(0), pc(0), vj(0),
        vk(0), qj(0), qk(0), rob_tag(0), waiting(0), result(0), branch_taken(0),
        branch_target(0) {}
};

class ReservationStation {
  static const int RS_SIZE = 32;

  RS_Entry cur_rs[RS_SIZE];
  RS_Entry next_rs[RS_SIZE];
  // 本周期 CDB 广播（MUX：推迟到 update 统一唤醒）。
  // 解决 do_issue 与 do_writeback 的执行顺序依赖——新发射的消费指令
  // 无论先发射还是后发射，只要与生产者广播同周期，都能被清掉 qj/qk。
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
    next_cdb_valid = false; // 每周期清零：本周期是否发生 CDB 广播
  }

  void update() {
    // CDB 广播 MUX：对最终 next_rs 统一唤醒，与各阶段执行顺序无关。
    // 注意：qj 与 qk 可能指向同一个生产者标签（如 add x3,x2,x2），
    // 必须用两个独立 if 而不是 else-if，否则只清一个、另一个死锁。
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

  // 发射时分配一个槽位（写 next_rs）
  void add(int op, int rob_tag, const DecodedInst &dec, uint32_t pc, int vj,
           int vk, int qj, int qk);

  // 读取条目（执行阶段需要 imm/pc/opcode 等信息，读 cur_rs）
  const RS_Entry &peek(int idx) const { return cur_rs[idx]; }

  // 计算结果并标记"已算完，等待 CDB 广播"（写 next_rs）
  void set_result(int idx, int result);

  // 分支/跳转结果：taken、target、以及 jal/jalr 的链接值 link_result（写
  // next_rs）
  void set_branch(int idx, bool taken, int target, int link_result);

  // 仅锁定条目（访存指令：地址已算好，等待内存结果，写 next_rs）
  void lock(int idx);

  // CDB 广播到来：只记录本周期广播（写 next_*），真正的唤醒推迟到
  // update() 的 MUX 统一执行（见 update()）。
  void set_broadcast(int producer_tag, uint32_t result_value);

  // 选择操作数就绪、未被锁定的条目用于执行（读 cur_rs）
  int select_ready(int &op, int &vj, int &vk, int &rob_tag);

  // 选择已算完、等待 CDB 广播的 ALU/分支条目（供 do_writeback 使用）
  int select_waiting(int &rob_tag, int &result);

  // 查找 rob_tag 对应的条目下标（读 cur_rs）
  int find_by_rob(int rob_tag);

  // 释放指定条目（写 next_rs）
  void release(int idx);

  void flush();

  // 发射阶段判断是否还有空闲槽位（读取 cur_rs）
  bool is_full() { return find_free_slot() == -1; }
};
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

  int find_free_slot();

public:
  void take_snapshot() {
    for (int i = 0; i < RS_SIZE; i++) {
      next_rs[i] = cur_rs[i];
    }
  }

  void update() {
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

  // CDB 广播到来，唤醒等待该标签的条目（读 cur_rs，写 next_rs）
  void wakeup(int producer_tag, int result_value);

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
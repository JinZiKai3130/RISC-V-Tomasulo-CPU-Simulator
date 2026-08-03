#pragma once
#include <cstdint>

class ROB {
  static const int ROB_SIZE = 8;
  struct ROBEntry {
    bool busy;
    int op;
    int dest_reg;
    uint32_t value; // 这里表示计算的结果，包括ALU的计算结果和load出来的数值
    bool ready;     // 判别是
    uint32_t pc;
    bool is_branch_taken;
    ROBEntry()
        : busy(0), op(0), dest_reg(0), value(0), ready(0), pc(0),
          is_branch_taken(0) {}
  };
  ROBEntry rob[ROB_SIZE];
  int head;  // 队首索引（提交位置）
  int tail;  // 队尾索引（分配位置）
  int count; // 当前有效条目数（用于判满/判空）

  int advance(int idx) { return (idx + 1) % ROB_SIZE; }

public:
  ROB() : head(0), tail(0), count(0) {}

  // ---------- 核心接口 ----------
  // 1. 分配：在尾部占个坑，返回分配到的条目索引（就是传给RS的 rob_tag）
  int allocate(uint8_t op_type, int dest_reg, uint32_t pc, bool pred_taken);

  // 2. 写回：CDB 广播到来，填入结果
  void writeback(int rob_tag, uint32_t value);

  // 3. 检查队首是否就绪（用于提交判断）
  bool is_head_ready() const;

  // 4. 获取队首条目（供提交阶段使用）
  ROBEntry *get_head_entry();

  // 5. 提交队首（真正写寄存器堆/内存，更新 head）
  // void commit_head(RegisterFile &reg_file, Memory &mem);

  // 6. 投机刷新：分支预测错误时，清空从给定 rob_tag 之后的所有年轻条目
  void flush(int rob_tag);

  bool is_full() const { return count == ROB_SIZE; }
  bool is_empty() const { return count == 0; }
};
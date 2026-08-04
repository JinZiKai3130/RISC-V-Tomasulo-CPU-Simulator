#pragma once
#include <cstdint>

struct ROBEntry {
  bool busy;
  int op;
  int dest_reg;
  uint32_t value; // 这里表示计算的结果，包括ALU的计算结果和load出来的数值
  bool ready;     // 判别是否算完，等待CDB广播序列，来自ALU和load
  uint32_t pc;
  bool is_branch_taken;
  ROBEntry()
      : busy(0), op(0), dest_reg(0), value(0), ready(0), pc(0),
        is_branch_taken(0) {}
};
class ROB {
  static const int ROB_SIZE = 8;

  ROBEntry cur_rob[ROB_SIZE];
  int cur_head;
  int cur_tail;
  int cur_count;

  ROBEntry next_rob[ROB_SIZE];
  // 本周期 issue/commit 的增量（take_snapshot 清零，update 时汇总到 cur）
  int next_issued;
  int next_committed;

  int advance(int idx) { return (idx + 1) % ROB_SIZE; }
  int advance_n(int idx, int n) { return (idx + n) % ROB_SIZE; }

public:
  ROB()
      : cur_head(0), cur_tail(0), cur_count(0), next_issued(0),
        next_committed(0) {}

  void take_snapshot() {
    for (int i = 0; i < ROB_SIZE; i++) {
      next_rob[i] = cur_rob[i];
    }
    next_issued = 0;
    next_committed = 0;
  }

  void update() {
    for (int i = 0; i < ROB_SIZE; i++) {
      cur_rob[i] = next_rob[i];
    }
    // 同一周期内 issue 与 commit 可以同时发生：增量汇总
    cur_head = advance_n(cur_head, next_committed);
    cur_tail = advance_n(cur_tail, next_issued);
    cur_count = cur_count + next_issued - next_committed;
  }

  // ---------- 核心接口 ----------
  // 1. 分配：在尾部占个坑，返回分配到的条目索引（就是传给RS的 rob_tag）
  int allocate(uint8_t op_type, int dest_reg, uint32_t pc);

  // 2. 写回：CDB 广播到来，填入结果
  void writeback(int rob_tag, uint32_t value);

  // 3. 检查队首是否就绪（用于提交判断）
  bool is_head_ready() const;

  // 4. 获取队首条目（供提交阶段使用）
  ROBEntry *get_head_entry();

  // 5. 提交队首（释放槽位，head 后移；寄存器/内存写入由 Simulator 完成）
  void commit_head();

  // 6. 投机刷新：分支预测错误时，清空从给定 rob_tag 之后的所有年轻条目
  void flush(int rob_tag);

  bool is_full() const { return cur_count == ROB_SIZE; }
  bool is_empty() const { return cur_count == 0; }
};
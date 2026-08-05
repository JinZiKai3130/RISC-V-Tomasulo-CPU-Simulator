#pragma once
#include <cstdint>

struct ROBEntry {
  bool busy;
  int op;
  int dest_reg;
  uint32_t value; // 这里表示计算的结果，包括ALU的计算结果和load出来的数值
  bool ready;     // 判断是否算完，等待commit，变化来自write_back操作
  uint32_t pc;
  bool is_branch_taken;
  // ---- 分支预测相关 ----
  bool pred_taken;    // 预测方向（issue 时写入）
  uint32_t pred_target;   // 预测目标 PC
  bool actual_taken;  // 实际方向（writeback 广播时写入）
  uint32_t actual_target; // 实际目标 PC
  ROBEntry()
      : busy(0), op(0), dest_reg(0), value(0), ready(0), pc(0),
        is_branch_taken(0), pred_taken(0), pred_target(0), actual_taken(0),
        actual_target(0) {}
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

  // 2b. 写回：分支指令把实际方向/目标填入 ROB（供 commit 时比较预测）
  void set_branch_actual(int rob_tag, bool taken, uint32_t target);

  // 2c. 发射：把预测方向/目标存入 ROB（供 commit 时比较）
  void set_prediction(int rob_tag, bool taken, uint32_t target);

  // 3. 检查队首是否就绪（用于提交判断）
  bool is_head_ready() const;

  // 4. 获取队首条目（供提交阶段使用）
  ROBEntry *get_head_entry();

  // 5. 提交队首（释放槽位，head 后移；寄存器/内存写入由 Simulator 完成）
  void commit_head();

  // 6. 投机刷新：分支预测错误时，清空所有未提交条目（整体重来）
  void flush();

  bool is_full() const { return cur_count == ROB_SIZE; }
  bool is_empty() const { return cur_count == 0; }
};
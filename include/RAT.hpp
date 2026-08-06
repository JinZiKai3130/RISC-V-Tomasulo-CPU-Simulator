#pragma once

class RegisterStatusTable {
private:
  static const int NUM_REGS = 32; // RV32I 共 32 个寄存器
  int cur_table[NUM_REGS];
  int next_table[NUM_REGS];
  // 本周期两个写源头的写使能（每周期在 take_snapshot 清零）。
  // 各阶段（issue/commit）只置位标志、互不写对方的 next_table 值，
  // 因此**与各阶段的执行顺序无关**；真正的仲裁在 update() 的 MUX 处完成。
  bool issue_write[NUM_REGS];  // issue 重命名了该寄存器
  bool commit_write[NUM_REGS]; // commit 想清除该寄存器

public:
  RegisterStatusTable() {
    for (int i = 0; i < NUM_REGS; i++) {
      cur_table[i] = -1;
      next_table[i] = -1;
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }

  void take_snapshot() {
    // 周期开始：next 暂存 cur（“无写”时的默认值），并清零两个写使能。
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = cur_table[i];
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }

  void update() {
    // 时钟沿：每个寄存器用 MUX 仲裁本周期真正写入的值。
    //   - issue 写了  → 保留 issue 的重命名（issue 优先，更新到更新的生产者）
    //   - 只有 commit 写了 → 清除为 -1（寄存器堆已更新，解除重命名）
    //   - 都没写 → 保持原值（next_table 里是 take_snapshot 复制的 cur）
    for (int i = 0; i < NUM_REGS; i++) {
      if (issue_write[i]) {
        // next_table[i] 已是 set_producer 写入的新 tag
      } else if (commit_write[i]) {
        next_table[i] = -1;
      }
      cur_table[i] = next_table[i];
    }
  }

  int get_producer(int reg) const {
    // 查找（读本周期输入 cur，issue 阶段使用）
    if (reg == 0)
      return -1;
    return cur_table[reg];
  }

  void set_producer(int reg, int rob_tag) {
    // issue 重命名：写入候选值 + 置 issue 写使能
    if (reg == 0)
      return; // x0 不可写
    next_table[reg] = rob_tag;
    issue_write[reg] = true;
  }

  void clear_if_match(int rob_tag) {
    // commit 清除：只置 commit 写使能，不写 next_table。
    // 判断用 cur_table（本周期输入，硬件上可读的当前值），
    // 绝不读 next_table（那是下一周期的值，硬件上读不到）。
    // 若同一周期该寄存器也被 issue 重命名（仅可能发生在 ROB 单条目
    // 槽位复用），MUX 在 update() 里选择 issue 的值。
    for (int i = 0; i < NUM_REGS; i++) {
      if (cur_table[i] == rob_tag) {
        commit_write[i] = true;
      }
    }
  }

  void flush() {
    // 分支预测错误：整体清空。注意必须连同写使能一起清——
    // 本周期 do_issue 可能刚为错误路径指令置了 issue_write，
    // 不清掉的话 update() 会把错误路径的重命名写入 cur。
    for (int i = 0; i < NUM_REGS; i++) {
      next_table[i] = -1;
      issue_write[i] = false;
      commit_write[i] = false;
    }
  }
};
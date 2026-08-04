#pragma once
#include "memory.hpp"
#include <cstdint>

struct LSQEntry {
  bool occupied;
  bool is_store;
  uint32_t funct3; // 访存宽度/符号（lb/lh/lw/lbu/lhu 与 sb/sh/sw）
  uint32_t addr;
  bool addr_ready;
  uint32_t value;  // Load 读回的值 或 Store 要写的值
  bool data_ready; // Store: 数据已就绪; Load: 值已就绪（内存完成/转发）
  uint32_t rob_index;
  bool rob_ready;   // 内存访问已完成（可通知 ROB / 可提交）
  bool broadcasted; // 完成状态已通知 ROB（load: 已广播; store: ROB 已标 ready）
  LSQEntry()
      : occupied(false), is_store(false), funct3(0), addr(0), addr_ready(false),
        value(0), data_ready(false), rob_index(0), rob_ready(false),
        broadcasted(false) {}
};

class LoadStoreQueue {
private:
  static const int SIZE = 8;
  LSQEntry cur_entries[SIZE];
  LSQEntry next_entries[SIZE];
  int cur_head;
  int cur_tail;
  int cur_count;      // 当前有效条目数
  int next_issued;    // 本周期发射（enqueue）增量
  int next_committed; // 本周期提交（dequeue）增量
  int cur_left_cycle;
  int cur_mem_execute_idx;
  int next_left_cycle;
  int next_mem_execute_idx;

  int advance(int idx) { return (idx + 1) % SIZE; }
  int advance_n(int idx, int n) { return (idx + n) % SIZE; }

public:
  LoadStoreQueue()
      : cur_head(0), cur_tail(0), cur_count(0), next_issued(0),
        next_committed(0), cur_left_cycle(0), cur_mem_execute_idx(-1),
        next_left_cycle(0), next_mem_execute_idx(-1) {}

  void take_snapshot() {
    for (int i = 0; i < SIZE; i++) {
      next_entries[i] = cur_entries[i];
    }
    next_issued = 0;
    next_committed = 0;
    next_left_cycle = cur_left_cycle;
    next_mem_execute_idx = cur_mem_execute_idx;
  }

  void update() {
    for (int i = 0; i < SIZE; i++) {
      cur_entries[i] = next_entries[i];
    }
    // 同一周期内发射与提交可同时发生：增量汇总
    cur_head = advance_n(cur_head, next_committed);
    cur_tail = advance_n(cur_tail, next_issued);
    cur_count = cur_count + next_issued - next_committed;
    cur_left_cycle = next_left_cycle;
    cur_mem_execute_idx = next_mem_execute_idx;
  }

  bool is_full() const { return cur_count == SIZE; }

  int allocate(bool is_store, int rob_tag, uint32_t funct3) {
    if (cur_count == SIZE)
      return -1;
    int slot = cur_tail;
    next_entries[slot] = LSQEntry();
    next_entries[slot].occupied = true;
    next_entries[slot].is_store = is_store;
    next_entries[slot].funct3 = funct3;
    next_entries[slot].rob_index = rob_tag;
    next_issued++;
    return slot;
  }

  void set_addr_by_rob(int rob_tag, uint32_t address) {
    // 由于CDB更新RS1，可以计算新的地址了，从ROB传过来
    int idx = find_by_rob(rob_tag);
    if (idx != -1) {
      next_entries[idx].addr = address;
      next_entries[idx].addr_ready = true;
    }
  }

  void set_data_by_rob(int rob_tag, uint32_t data) {
    // 这里是由于CDB广播的更新，而产生的更新操作，从ROB传来
    int idx = find_by_rob(rob_tag);
    if (idx != -1) {
      next_entries[idx].value = data;
      next_entries[idx].data_ready = true;
    }
  }

  int check_forward(int load_rob_tag, uint32_t load_addr) {
    // 检查当前指令之前是否有指令还在写
    int load_idx = find_by_rob(load_rob_tag);
    if (load_idx == -1)
      return -1;

    int idx = cur_head;
    while (idx != load_idx) {
      if (cur_entries[idx].occupied && cur_entries[idx].is_store &&
          cur_entries[idx].addr_ready && cur_entries[idx].data_ready) {
        if (cur_entries[idx].addr == load_addr) {
          next_entries[load_idx].value = cur_entries[idx].value;
          next_entries[load_idx].data_ready = true;
          return idx;
        }
      }
      idx = advance(idx);
    }
    return -1;
  }

  // ---- 每周期推进内存单元（串行：同一时刻最多一条访存，读 cur 写 next）----
  void step(Memory &mem) {
    // 1. 内存单元忙：倒计时
    if (cur_left_cycle > 0) {
      next_left_cycle = cur_left_cycle - 1;
      if (cur_left_cycle == 1) {
        // 本次访问完成
        int idx = cur_mem_execute_idx;
        if (idx >= 0 && idx < SIZE && cur_entries[idx].occupied) {
          if (cur_entries[idx].is_store) {
            // store：仅标记"内存访问完成"，真正写内存留到 commit（投机安全）
            next_entries[idx].rob_ready = true;
          } else {
            // load：先尝试从更老的 store 转发，否则读内存
            uint32_t addr = cur_entries[idx].addr;
            if (check_forward(cur_entries[idx].rob_index, addr) == -1) {
              next_entries[idx].value =
                  read_mem(mem, addr, cur_entries[idx].funct3);
              next_entries[idx].data_ready = true;
            }
            next_entries[idx].rob_ready = true;
          }
        }
        next_mem_execute_idx = -1;
      }
      return;
    }
    // 2. 内存单元空闲：启动一条就绪的访存
    int idx = select_mem_ready();
    if (idx == -1)
      return;
    next_left_cycle = 3;
    next_mem_execute_idx = idx;
  }

  // 找一个"内存访问完成、等待 CDB 广播"的 load（返回下标，无则 -1）
  int find_done_load() {
    int n = cur_count;
    int idx = cur_head;
    while (n > 0) {
      if (cur_entries[idx].occupied && !cur_entries[idx].is_store &&
          cur_entries[idx].rob_ready && !cur_entries[idx].broadcasted)
        return idx;
      idx = advance(idx);
      n--;
    }
    return -1;
  }

  // 找一个"内存访问完成、等待通知 ROB"的 store（返回下标，无则 -1）
  int find_done_store() {
    int n = cur_count;
    int idx = cur_head;
    while (n > 0) {
      if (cur_entries[idx].occupied && cur_entries[idx].is_store &&
          cur_entries[idx].rob_ready && !cur_entries[idx].broadcasted)
        return idx;
      idx = advance(idx);
      n--;
    }
    return -1;
  }

  // 访问器（供 do_writeback 使用）
  int get_rob_index(int idx) const { return cur_entries[idx].rob_index; }
  uint32_t get_value(int idx) const { return cur_entries[idx].value; }
  void mark_broadcasted(int idx) { next_entries[idx].broadcasted = true; }

  // 队首提交：load 释放槽位；store 真正写内存并释放。返回是否提交成功。
  bool commit_head(Memory &mem) {
    if (!can_commit_head())
      return false;
    auto &entry = cur_entries[cur_head];
    if (entry.is_store) {
      write_mem(mem, entry.addr, entry.value, entry.funct3); // 真正写内存
    }
    next_entries[cur_head] = LSQEntry(); // 释放队首
    next_committed++;
    return true;
  }

  bool can_commit_head() {
    if (cur_count == 0)
      return false;
    auto &e = cur_entries[cur_head];
    if (e.is_store)
      return e.addr_ready && e.data_ready && e.rob_ready;
    return e.addr_ready && e.data_ready;
  }

  void flush() {
    // TODO(分支预测)：清空错误路径的访存条目，保留在途 load。
    // 当前先整体清空，作为占位。
    for (int i = 0; i < SIZE; i++) {
      next_entries[i] = LSQEntry();
    }
    next_issued = 0;
    next_committed = cur_count;
    next_left_cycle = 0;
    next_mem_execute_idx = -1;
  }

private:
  int find_by_rob(int rob_tag) {
    int n = cur_count;
    int idx = cur_head;
    while (n > 0) {
      if (cur_entries[idx].occupied &&
          (int)cur_entries[idx].rob_index == rob_tag) {
        return idx;
      }
      idx = advance(idx);
      n--;
    }
    return -1;
  }

  int select_mem_ready() {
    // 从队首开始找最老的一条"可开始访存"的条目（跳过未就绪者，避免死锁）
    int n = cur_count;
    int idx = cur_head;
    while (n > 0) {
      if (cur_entries[idx].occupied) {
        if (cur_entries[idx].is_store) {
          if (cur_entries[idx].addr_ready && cur_entries[idx].data_ready &&
              !cur_entries[idx].rob_ready)
            return idx;
        } else {
          if (cur_entries[idx].addr_ready && !cur_entries[idx].rob_ready)
            return idx;
        }
      }
      idx = advance(idx);
      n--;
    }
    return -1;
  }

  // 按宽度/符号读内存（对应 naive 的 load 逻辑）
  uint32_t read_mem(Memory &mem, uint32_t addr, uint32_t funct3) {
    switch (funct3) {
    case 0x0: // lb
      return (uint32_t)(int32_t)(int8_t)mem.read_byte(addr);
    case 0x1: // lh
      return (uint32_t)(int32_t)(int16_t)(mem.read_byte(addr) |
                                          (mem.read_byte(addr + 1) << 8));
    case 0x2: // lw
      return mem.read_word(addr);
    case 0x4: // lbu
      return mem.read_byte(addr);
    case 0x5: // lhu
      return mem.read_byte(addr) | (mem.read_byte(addr + 1) << 8);
    }
    return 0;
  }

  // 按宽度写内存（对应 naive 的 store 逻辑）
  void write_mem(Memory &mem, uint32_t addr, uint32_t value, uint32_t funct3) {
    switch (funct3) {
    case 0x0: // sb
      mem.write_byte(addr, value & 0xFF);
      break;
    case 0x1: // sh
      mem.write_byte(addr, value & 0xFF);
      mem.write_byte(addr + 1, (value >> 8) & 0xFF);
      break;
    case 0x2: // sw
      mem.write_word(addr, value);
      break;
    }
  }
};
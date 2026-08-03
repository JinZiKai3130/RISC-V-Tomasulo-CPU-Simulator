#pragma once
#include "memory.hpp"
#include <cstdint>

struct LSQEntry {
  bool occupied;
  bool is_store;
  uint32_t addr;
  bool addr_ready;
  uint32_t value;     // Load读到的值 或 Store要写的值
  bool data_ready;    // Store: 要存的Vk值到了; Load: 从内存/转发读取的数据到了
  uint32_t rob_index; // 绑定的 ROB 条目编号
  bool rob_ready;
};

class LoadStoreQueue {
private:
  static const int SIZE = 8;
  LSQEntry entries[SIZE];
  int head;
  int tail;
  int count; // 当前有效条目数
  int left_cycle;
  int mem_execute_idx;

  int advance(int idx) { return (idx + 1) % SIZE; }

public:
  LoadStoreQueue() : head(0), tail(0), count(0), left_cycle(0) {}

  int allocate(bool is_store, int rob_tag) {
    if (count == SIZE)
      return -1;
    int slot = tail;
    entries[slot].occupied = true;
    entries[slot].is_store = is_store;
    entries[slot].rob_index = rob_tag;
    entries[slot].addr_ready = false;
    entries[slot].data_ready = false;
    entries[slot].addr = 0;
    entries[slot].value = 0;
    entries[slot].rob_ready = false;
    tail = advance(tail);
    count++;
    return slot;
  }

  void set_addr_by_rob(int rob_tag, uint32_t address) {
    // 由于CDB更新RS1，可以计算新的地址了，从ROB传过来
    int idx = find_by_rob(rob_tag);
    if (idx != -1) {
      entries[idx].addr = address;
      entries[idx].addr_ready = true;
    }
  }

  void set_data_by_rob(int rob_tag, uint32_t data) {
    // 这里是由于CDB广播的更新，而产生的更新操作，从ROB传来
    int idx = find_by_rob(rob_tag);
    if (idx != -1) {
      entries[idx].value = data;
      entries[idx].data_ready = true;
    }
  }

  int check_forward(int load_rob_tag, uint32_t load_addr) {
    int load_idx = find_by_rob(load_rob_tag);
    if (load_idx == -1)
      return -1;

    int idx = head;
    while (idx != load_idx) {
      if (entries[idx].occupied && entries[idx].is_store &&
          entries[idx].addr_ready && entries[idx].data_ready) {
        if (entries[idx].addr == load_addr) {
          entries[load_idx].value = entries[idx].value;
          entries[load_idx].data_ready = true;
          return idx;
        }
      }
      idx = advance(idx);
    }
    return -1;
  }

  bool can_commit_head() {
    if (count == 0 || left_cycle != 0)
      return false;
    auto &entry = entries[head];
    return entry.addr_ready && entry.data_ready;
  }

  void commit_head(Memory &mem) {
    if (!can_commit_head())
      return;
    auto &entry = entries[head];

    entry.occupied = false;
    head = advance(head);
    count--;
  }

  void execute_write(Memory &mem, int idx) {
    auto &entry = entries[idx];
    mem.write(entry.addr, entry.value);
    left_cycle = 3;
    mem_execute_idx = head;
  }

  void execute_load(Memory &mem) {
    int idx = head;
    if (idx == -1 || entries[idx].is_store)
      return;
    auto &entry = entries[idx];

    // 这里应该有多种操作
    entry.value = mem.read(entry.addr);

    mem_execute_idx = idx;
  }

  void tick(Memory &mem) {
    if (left_cycle == 0 && !entries[head].is_store) {
      execute_load(mem);
      left_cycle = 3;
      return;
    }
    for (int i = 0; i < SIZE; i++) {
      if (left_cycle == 0 && entries[i].rob_ready) {
        execute_write(mem, i);
        left_cycle = 3;
        return;
      }
    }
    if (left_cycle != 0) {
      left_cycle--;
      if (left_cycle == 0 && !entries[head].is_store) {
        entries[head].data_ready = true; // 数据就绪，可以准备提交了
        // CDB操作
        commit_head(mem);
      }
    }
  }

  void flush() {
    int leave_idx = -1;
    if (entries[head].occupied && !entries[head].is_store && left_cycle > 0) {
      leave_idx = head;
    }
    for (int i = 0; i < SIZE; i++) {
      if (leave_idx == i)
        continue;
      entries[i].occupied = false;
    }
    if (leave_idx == -1)
      head = tail = count = 0;
    else {
      head = leave_idx;
      tail = advance(leave_idx);
      count = 1;
    }
    // 但是这里保留cycleleft，因为最后一步可能还在写
  }

private:
  int find_by_rob(int rob_tag) {
    int idx = head;
    int temp_count = count;
    while (temp_count > 0) {
      if (entries[idx].occupied && entries[idx].rob_index == rob_tag) {
        return idx;
      }
      idx = advance(idx);
      temp_count--;
    }
    return -1;
  }
};
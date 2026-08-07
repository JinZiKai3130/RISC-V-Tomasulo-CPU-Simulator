#pragma once
#include "memory.hpp"
#include <cstdint>

struct LSQEntry {
  bool occupied;
  bool is_store;
  uint32_t funct3;
  uint32_t addr;
  bool addr_ready;
  uint32_t value;  // load存储读出来的值，store存储写入的值
  bool data_ready; // load已读出，store写入值准备就绪
  uint32_t rob_index;
  bool rob_ready; // load已读出 等待广播
  bool
      broadcasted; // load表示已经广播，store表示ready了，等待到达队头执行写入memory
  bool committed;  // store到达ROB队首，可以进行写入memory操作
  bool write_done; // store写内存已完成
  LSQEntry();
};

class LoadStoreQueue {
private:
  static const int SIZE = 8;
  LSQEntry cur_entries[SIZE];
  LSQEntry next_entries[SIZE];
  int cur_head;
  int cur_tail;
  int cur_count;
  int next_issued;
  int next_committed;
  int cur_left_cycle;
  int cur_mem_execute_idx; // 当前正在修改的idx
  int next_left_cycle;
  int next_mem_execute_idx;

  int advance(int idx) const;
  int advance_n(int idx, int n) const;

public:
  LoadStoreQueue();

  void take_snapshot();

  void update();

  bool is_full() const;

  int allocate(bool is_store, int rob_tag, uint32_t funct3);

  void set_addr_by_rob(int rob_tag, uint32_t address);

  void set_data_by_rob(int rob_tag, uint32_t data);

  void step(Memory &mem);

  int find_done_load();

  int find_ready_store();

  int get_rob_index(int idx) const;
  uint32_t get_value(int idx) const;
  void mark_broadcasted(int idx);

  bool commit_head();

  bool is_head_write_done() const;

  void commit_store_head();

  bool start_commit_head();

  void flush();

private:
  int find_by_rob(int rob_tag);

  int select_mem_ready();

  bool has_older_unfinished_store(int load_idx);

  uint32_t read_mem(Memory &mem, uint32_t addr, uint32_t funct3);

  void write_mem(Memory &mem, uint32_t addr, uint32_t value, uint32_t funct3);
};
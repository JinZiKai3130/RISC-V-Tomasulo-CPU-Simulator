#include "../include/LSQ.hpp"

LSQEntry::LSQEntry()
    : occupied(false), is_store(false), funct3(0), addr(0), addr_ready(false),
      value(0), data_ready(false), rob_index(0), rob_ready(false),
      broadcasted(false), committed(false), write_done(false) {}

LoadStoreQueue::LoadStoreQueue()
    : cur_head(0), cur_tail(0), cur_count(0), next_issued(0), next_committed(0),
      cur_left_cycle(0), cur_mem_execute_idx(-1), next_left_cycle(0),
      next_mem_execute_idx(-1) {}

int LoadStoreQueue::advance(int idx) const { return (idx + 1) % SIZE; }
int LoadStoreQueue::advance_n(int idx, int n) const { return (idx + n) % SIZE; }

void LoadStoreQueue::take_snapshot() {
  for (int i = 0; i < SIZE; i++) {
    next_entries[i] = cur_entries[i];
  }
  next_issued = 0;
  next_committed = 0;
  next_left_cycle = cur_left_cycle;
  next_mem_execute_idx = cur_mem_execute_idx;
}

void LoadStoreQueue::update() {
  for (int i = 0; i < SIZE; i++) {
    cur_entries[i] = next_entries[i];
  }
  cur_head = advance_n(cur_head, next_committed);
  cur_tail = advance_n(cur_tail, next_issued);
  cur_count = cur_count + next_issued - next_committed;
  cur_left_cycle = next_left_cycle;
  cur_mem_execute_idx = next_mem_execute_idx;
}

bool LoadStoreQueue::is_full() const { return cur_count == SIZE; }

int LoadStoreQueue::allocate(bool is_store, int rob_tag, uint32_t funct3) {
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

void LoadStoreQueue::set_addr_by_rob(int rob_tag, uint32_t address) {
  int idx = find_by_rob(rob_tag);
  if (idx != -1) {
    next_entries[idx].addr = address;
    next_entries[idx].addr_ready = true;
  }
}

void LoadStoreQueue::set_data_by_rob(int rob_tag, uint32_t data) {
  int idx = find_by_rob(rob_tag);
  if (idx != -1) {
    next_entries[idx].value = data;
    next_entries[idx].data_ready = true;
  }
}

void LoadStoreQueue::step(Memory &mem) {
  if (cur_left_cycle > 0) {
    next_left_cycle = cur_left_cycle - 1;
    if (cur_left_cycle == 1) {
      int idx = cur_mem_execute_idx;
      if (idx >= 0 && idx < SIZE && cur_entries[idx].occupied) {
        if (cur_entries[idx].is_store) {
          write_mem(mem, cur_entries[idx].addr, cur_entries[idx].value,
                    cur_entries[idx].funct3);
          next_entries[idx].write_done = true;
        } else {
          uint32_t addr = cur_entries[idx].addr;
          next_entries[idx].value =
              read_mem(mem, addr, cur_entries[idx].funct3);
          next_entries[idx].data_ready = true;
          next_entries[idx].rob_ready = true;
        }
      }
      next_mem_execute_idx = -1;
    }
    return;
  }
  int idx = select_mem_ready();
  if (idx == -1)
    return;
  next_left_cycle = 3;
  next_mem_execute_idx = idx;
}

int LoadStoreQueue::find_done_load() {
  int idx = cur_head;
  for (int i = 0; i < cur_count; i++) {
    if (cur_entries[idx].occupied && !cur_entries[idx].is_store &&
        cur_entries[idx].rob_ready && !cur_entries[idx].broadcasted)
      return idx;
    idx = advance(idx);
  }
  return -1;
}

int LoadStoreQueue::find_ready_store() {
  int idx = cur_head;
  for (int i = 0; i < cur_count; i++) {
    if (cur_entries[idx].occupied && cur_entries[idx].is_store &&
        cur_entries[idx].addr_ready && cur_entries[idx].data_ready &&
        !cur_entries[idx].broadcasted)
      return idx;
    idx = advance(idx);
  }
  return -1;
}

int LoadStoreQueue::get_rob_index(int idx) const {
  return cur_entries[idx].rob_index;
}
uint32_t LoadStoreQueue::get_value(int idx) const {
  return cur_entries[idx].value;
}
void LoadStoreQueue::mark_broadcasted(int idx) {
  next_entries[idx].broadcasted = true;
}

bool LoadStoreQueue::commit_head() {
  // 这里用来commit关于load的东西
  if (cur_count == 0)
    return false;
  auto &e = cur_entries[cur_head];
  if (e.is_store)
    return false;
  if (!(e.addr_ready && e.data_ready))
    return false;
  next_entries[cur_head] = LSQEntry();
  next_committed++;
  return true;
}

bool LoadStoreQueue::is_head_write_done() const {
  if (cur_count == 0)
    return false;
  auto &e = cur_entries[cur_head];
  return e.occupied && e.is_store && e.write_done;
}

void LoadStoreQueue::commit_store_head() {
  // commit store
  if (cur_count == 0)
    return;
  auto &e = cur_entries[cur_head];
  if (!e.occupied || !e.is_store || !e.write_done)
    return;
  next_entries[cur_head] = LSQEntry();
  next_committed++;
}

bool LoadStoreQueue::start_commit_head() {
  if (cur_count == 0)
    return false;
  auto &e = cur_entries[cur_head];
  if (!e.occupied || !e.is_store || e.committed)
    return false;
  if (!(e.addr_ready && e.data_ready))
    return false;
  next_entries[cur_head].committed = true;
  return true;
}

void LoadStoreQueue::flush() {
  for (int i = 0; i < SIZE; i++) {
    next_entries[i] = LSQEntry();
  }
  next_issued = 0;
  next_committed = cur_count;
  next_left_cycle = 0;
  next_mem_execute_idx = -1;
}

int LoadStoreQueue::find_by_rob(int rob_tag) {
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

int LoadStoreQueue::select_mem_ready() {
  // 从队首开始找可以进行内存读写操作的指令
  int idx = cur_head;
  for (int i = 0; i < cur_count; i++) {
    if (cur_entries[idx].occupied) {
      if (cur_entries[idx].is_store) {
        // 已经commit即store_start了，但是还没写完
        if (cur_entries[idx].committed && !cur_entries[idx].write_done)
          return idx;
      } else {
        // 如果要做load确保所有的store已经写完了
        if (cur_entries[idx].addr_ready && !cur_entries[idx].rob_ready &&
            !has_older_unfinished_store(idx))
          return idx;
      }
    }
    idx = advance(idx);
  }
  return -1;
}

bool LoadStoreQueue::has_older_unfinished_store(int load_idx) {
  int idx = cur_head;
  for (int i = 0; i < cur_count; i++) {
    if (load_idx == idx)
      break;
    if (cur_entries[idx].occupied && cur_entries[idx].is_store)
      return true;
    idx = advance(idx);
  }
  return false;
}

uint32_t LoadStoreQueue::read_mem(Memory &mem, uint32_t addr, uint32_t funct3) {
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

void LoadStoreQueue::write_mem(Memory &mem, uint32_t addr, uint32_t value,
                               uint32_t funct3) {
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

#pragma once
struct RS_Entry {
  bool busy; // 要里面还存有数据，就是busy状态
  int op;
  int vj, vk;
  int qj, qk;
  int rob_tag;  // ROB的index
  bool waiting; // 这里表示CDB是否正常广播
  int result;
  RS_Entry()
      : busy(0), op(0), vj(0), vk(0), qj(0), qk(0), rob_tag(0), waiting(0) {}
};

class ReservationStation {
  static const int RS_SIZE = 8;

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

  void add(int op, int rob_tag, int vj, int vk, int qj, int qk);

  void wakeup(int producer_tag, int result_value);

  int select_ready(int &op, int &vj, int &vk, int &rob_tag);

  void flush();
};
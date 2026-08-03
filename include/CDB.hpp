#pragma once

struct CDB_Packet {
  int tag;
  int value;
};
class CDB {
  static const int SIZE = 8;
  CDB_Packet cdb_list[SIZE];

public:
};
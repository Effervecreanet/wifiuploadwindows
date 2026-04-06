

struct tx_stats {
  u_char      curr_percent;
  u_char      curr_percent_bak;
  uint64_t     one_percent;
  SYSTEMTIME  start;
  SYSTEMTIME  current;
  SYSTEMTIME  currentbak;
  SYSTEMTIME  remaining;
  SYSTEMTIME  end;
  SYSTEMTIME  elapsed;
  uint64_t     total_size;
  uint64_t     received_size;
  uint64_t     received_size_bak;
  u_int       average_rate;
};

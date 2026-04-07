
#define FILENAME_MAX_SIZE  512

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


struct success_info {
	char filename[FILENAME_MAX_SIZE];
	char filenameSize[24];
	char elapsedTime[24];
	char averagespeed[20];
};

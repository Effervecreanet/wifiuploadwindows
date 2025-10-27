

struct tx_stats {
	u_char      curr_percent;
	u_char      curr_percent_bak;
	u_int64     one_percent;
	SYSTEMTIME  start;
	SYSTEMTIME  current;
	SYSTEMTIME  currentbak;
	SYSTEMTIME  remaining;
	SYSTEMTIME  end;
	SYSTEMTIME  elapsed;
	u_int64     total_size;
	u_int64     received_size;
	u_int64     received_size_bak;
	u_int       average_rate;
};

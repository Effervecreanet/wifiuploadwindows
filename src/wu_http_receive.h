
#define FILENAME_MAX_SIZE  512

struct user_stats {
  char filename[FILENAME_MAX_SIZE];
  char filesize[32];
  char elapsedtime[32];
  char averagespeed[32];
};

void print_upload_info(struct tx_stats* txstats, COORD coordAverageTX, COORD* cursorPosition, COORD coordPerCent);
// void chrono(struct success_info* successinfo, DWORD tick_start, u_int64 sizeNewFile);
int receive_file(COORD *cursorPosition, struct header_nv *httpnv, int s, struct user_stats *upstats, int theme, int *bytesent);


#define FILENAME_MAX_SIZE  512

struct user_stats {
  char filename[FILENAME_MAX_SIZE];
  char filesize[32];
  char elapsedtime[32];
  char averagespeed[32];
};

int receiveFile(HANDLE conScreenBuffer, COORD *cursorPosition,
                struct header_nv *httpnv, int s,
                struct user_stats *upstats, int theme);

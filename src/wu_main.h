#define LOG_DIRECTORY ".wulogs"

void WriteConsoleA_INF(HANDLE conScreenBuffer, enum idmsg id, void *p);
void clearTXRXPane(HANDLE conScreenBuffer, COORD* cursorPosition);
int CreateDownloadDirectory(unsigned char dd[1024]);

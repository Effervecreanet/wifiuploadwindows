#define LOG_DIRECTORY ".wulogs"

void WriteConsoleA_INFO(enum idmsg id, void *p);
void clearTXRXPane(COORD* cursorPosition);
int CreateDownloadDirectory(unsigned char dd[1024]);

#define LOG_DIRECTORY ".wulogs"

void WriteConsoleA_INFO(enum idmsg id, void *p);
void clearTXRXPane(COORD* cursorPosition);
void create_download_directory(unsigned char dd[1024]);

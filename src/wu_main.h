#define LOG_DIRECTORY ".wulogs"

void write_info_in_console(enum idmsg id, void *p, DWORD err);
void clear_txrx_pane(COORD* cursorPosition);
void create_download_directory(char dd[1024]);

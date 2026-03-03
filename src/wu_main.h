#define LOG_DIRECTORY ".wulogs"

void build_download_directory(char dd[1024]);
void write_info_in_console(enum idmsg id, void *p, DWORD err);
void show_error_wait_close(COORD *cursorPosition, enum idmsg id, const void* p, DWORD err);
void clear_txrx_pane(COORD* cursorPosition);
void create_download_directory(char dd[1024]);

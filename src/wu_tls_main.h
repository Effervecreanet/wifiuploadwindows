struct paramThread {
	struct in_addr inaddr;
	COORD cursorPosition[2];
};


DWORD WINAPI wu_tls_loop(struct paramThread *prThread);
void https_wu_quit_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, SOCKET s_user, int* bytesent);
void https_quit_wu(void);

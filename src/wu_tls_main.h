struct paramThread {
	struct in_addr inaddr;
	COORD cursorPosition[2];
};


DWORD WINAPI wu_tls_loop(struct paramThread *prThread);

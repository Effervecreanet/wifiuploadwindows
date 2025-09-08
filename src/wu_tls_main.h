struct paramThread {
	struct in_addr inaddr;
	COORD cursorPosition;
};


DWORD WINAPI wu_tls_loop(struct paramThread *prThread);

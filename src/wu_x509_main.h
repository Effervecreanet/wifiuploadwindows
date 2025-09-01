struct paramThread {
	struct in_addr inaddr;
	COORD cursorPosition;
};


DWORD WINAPI wu_x509_func(struct paramThread *prThread);

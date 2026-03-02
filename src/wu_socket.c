#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"
#define LISTEN_PORT 80
#define LISTEN_HTTPS_PORT 443

extern int* g_listensocket;
extern int* g_listenhttpssocket;
extern HANDLE g_hConsoleOutput;

static void
socket_show_error_wait_close(COORD *cusorPosition, const char* message) {
	CHAR Buffer[1024];
	DWORD written;

	ZeroMemory(Buffer, 1024);
	StringCchPrintf(Buffer, 1024, message, WSAGetLastError());

	cusorPosition->Y++;

	SetConsoleCursorPosition(g_hConsoleOutput, *cusorPosition);
	WriteConsoleA(g_hConsoleOutput, Buffer, (DWORD)strlen(Buffer), &written, NULL);

	for (;;) Sleep(10000);
}

int
create_socket(COORD* cursorPosition) {
	int s;
	INPUT_RECORD inRec;
	DWORD read;

	s = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_1);

	return s;
}

void
bind_socket(COORD* cursorPosition, int s, struct in_addr inaddr) {
	struct sockaddr_in sainServer;
	char tval = 1;

	ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
	sainServer.sin_addr = inaddr;
	sainServer.sin_family = AF_INET;
	sainServer.sin_port = htons(LISTEN_PORT);

	if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in)))
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_2);

	listen(s, 10);

	g_listensocket = &s;

	return;
}

void
bind_socket2(COORD* cursorPosition, int s, struct in_addr inaddr) {
	struct sockaddr_in sainServer;

	ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
	sainServer.sin_addr = inaddr;
	sainServer.sin_family = AF_INET;
	sainServer.sin_port = htons(LISTEN_HTTPS_PORT);

	if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in)))
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_2);
	
	listen(s, 100);

	g_listenhttpssocket = &s;

	return;
}
int
accept_conn(COORD* cursorPosition, int s, char ipaddrstr[16]) {
	int s_user, sainLen;
	struct sockaddr_in sainUser;

	ZeroMemory(&sainUser, sizeof(struct sockaddr_in));
	sainLen = sizeof(struct sockaddr);

	for (;;) {
		s_user = (int)accept(s, (struct sockaddr*)&sainUser, &sainLen);

		if (s_user == INVALID_SOCKET)
			socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_3);
		}
		else {
			char* ipstr;
			ipstr = inet_ntoa(sainUser.sin_addr);
			memcpy(ipaddrstr, ipstr, strlen(ipstr));
			break;
		}
	}


	return s_user;
}




#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"

#define LISTEN_PORT 80

extern SOCKET g_listensocket;
extern HANDLE g_hConsoleOutput;

/*
 * Function description:
 * - Create a tcp/ip socket, write a message in console if an error occurs.
 * Arguments:
 * - cursorPosition: Position of error message if any.
 * Return value:
 * - s: New socket.
 */
SOCKET
create_socket(COORD* cursorPosition) {
	SOCKET s;
	INPUT_RECORD inRec;
	DWORD read;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET) {
		CHAR Buffer[1024];
		DWORD written;

		ZeroMemory(Buffer, 1024);

		StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_1, WSAGetLastError());
		cursorPosition->Y++;

		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		WriteConsoleA(g_hConsoleOutput, Buffer, (DWORD)strlen(Buffer), &written, NULL);

		for (;;)
			Sleep(10000);
	}

	return s;
}

/*
 * Function description:
 * - Bind the new socket to an ip address. Start listening with the new binded socket.
 * Arguments:
 * - cursorPosition: Position where print an error message if any
 * - s: New server socket.
 * - inaddr: IP address the socket bind to.
 */
void
bind_socket(COORD* cursorPosition, SOCKET s, struct in_addr inaddr) {
	struct sockaddr_in sainServer;
	char tval = 1;

	ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
	sainServer.sin_addr = inaddr;
	sainServer.sin_family = AF_INET;
	sainServer.sin_port = htons(LISTEN_PORT);

	if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in))) {
		CHAR Buffer[1024];
		DWORD written;

		ZeroMemory(Buffer, 1024);

		StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_2, WSAGetLastError());
		cursorPosition->Y++;

		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		WriteConsoleA(g_hConsoleOutput, Buffer, (DWORD)strlen(Buffer), &written, NULL);

		for (;;)
			Sleep(10000);
	}

	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&tval, sizeof(char));

	listen(s, 10);

	g_listensocket = s;

	return;
}

/*
 * Function description:
 * - Accept new incoming connection write a error message if an error occurs.
 * Arguments:
 * - cursorPosition: Position where to print error message.
 * - s: Socket server that accept new incoming connection.
 * - ipaddrstr[16]: User or client IP address that initialize new incoming
 *   connection.
 * Return value:
 * - s_user: New incoming connection socket.
 */
SOCKET
accept_conn(COORD* cursorPosition, SOCKET s, char ipaddrstr[16]) {
	SOCKET s_user;
	int sainLen;
	struct sockaddr_in sainUser;

	ZeroMemory(&sainUser, sizeof(struct sockaddr_in));
	sainLen = sizeof(struct sockaddr);

	for (;;) {
		s_user = accept(s, (struct sockaddr*)&sainUser, &sainLen);

		if (s_user == INVALID_SOCKET) {
			CHAR Buffer[1024];
			DWORD written;

			ZeroMemory(Buffer, 1024);

			StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_3, WSAGetLastError());
			cursorPosition->Y++;

			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			WriteConsoleA(g_hConsoleOutput, Buffer, (DWORD)strlen(Buffer), &written, NULL);

			Sleep(100);
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




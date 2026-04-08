#include <WS2tcpip.h>
#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"

#pragma comment(lib, "ws2_32.lib")

#define LISTEN_PORT 80
#define LISTEN_HTTPS_PORT 443

extern SOCKET g_listensocket;
extern SOCKET g_listenhttpssocket;
extern SOCKET g_usersocket;
extern HANDLE g_hConsoleOutput;

static void socket_show_error_wait_close(COORD *cusorPosition, const char* message);

/*
 * Function description:
 *  - Show winsock error message in console and wait user to close the program.
 * Arguments:
 *  - cursorPosition: Console cursor position where wu writes info or errors.
 *  - message: Message to write in console. It must be formattable with WSAGetLastError() output.
 */
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

/*
 * Function description:
 * - Create a socket.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes winsock error.
 * Return value:
 * - Socket created or/and if socket creation failed, the function does not return and show error message in console.
 */

SOCKET
create_socket(COORD* cursorPosition) {
	SOCKET s;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_1);

	return s;
}

/*
 * Function description:
 * Bind a socket to the PC IPv4 address and listen HTTP request on this socket.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes winsock error.
 * - s: Socket to bind and listen.
 * - inaddr: IPv4 address to bind.
 */

void
bind_socket(COORD* cursorPosition, SOCKET s, struct in_addr inaddr) {
	struct sockaddr_in sainServer;

	ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
	sainServer.sin_addr = inaddr;
	sainServer.sin_family = AF_INET;
	sainServer.sin_port = htons(LISTEN_PORT);

	if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in)))
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_2);

	listen(s, 10);

	g_listensocket = s;

	return;
}

/*
 * Function description:
 * - Bind a socket to the PC IPv4 address and listen HTTPS request on this socket.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes winsock error.
 * - s: Socket to bind and listen.
 * - inaddr: IPv4 address to bind.
 */
void
bind_socket2(COORD* cursorPosition, SOCKET s, struct in_addr inaddr) {
	struct sockaddr_in sainServer;

	ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
	sainServer.sin_addr = inaddr;
	sainServer.sin_family = AF_INET;
	sainServer.sin_port = htons(LISTEN_HTTPS_PORT);

	if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in)))
		socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_2);
	
	listen(s, 100);

	g_listenhttpssocket = s;

	return;
}

/*
 * Function description:
 * - Accept a connection on a socket and get client IP address.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes winsock error.
 * - s: Socket to accept connection on.
 * - ipaddrstr: Buffer to store client IP address string.
 * Return value:
 * - Socket of accepted connection or/and if accept failed, the function does not
 *   return and show error message in console.
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
			socket_show_error_wait_close(cursorPosition, ERROR_MESSAGE_SOCKET_3);
		}
		else {
			InetNtop(AF_INET, &sainUser.sin_addr, ipaddrstr, 16);
			break;
		}
	}

	g_usersocket = s_user;

	return s_user;
}




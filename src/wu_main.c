#include <Windows.h>
#include <WinSock.h>
#include <iphlpapi.h>
#include <UserEnv.h>
#include <strsafe.h>
#include <stdio.h>
#include <conio.h>
#include <direct.h>
#include <locale.h>
#include <sys\stat.h>


#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_loop.h"
#include "wu_available_address.h"


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "userenv.lib")

extern struct wu_msg wumsg[];
FILE* g_fplog;
int* g_listensocket;
int* g_usersocket;
HANDLE g_hConsoleOutput;
HANDLE g_hNewFile_tmp;
unsigned char g_sNewFile_tmp[1024];

/*
 * Function description:
 * - Routine windows OS arise when the user close the app window or the user
 *   log out or user shutdown the PC.
 * Arguments:
 * - dwCtrlType: Type of event Windows OS arise.
 */
BOOL WINAPI
HandlerRoutine(_In_ DWORD dwCtrlType)
{
	switch (dwCtrlType) {
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (g_fplog != NULL)
			fclose(g_fplog);
		if (g_usersocket != NULL && *g_usersocket != 0)
			closesocket(*g_usersocket);
		if (g_listensocket != NULL)
			closesocket(*g_listensocket);
		if (g_hConsoleOutput != INVALID_HANDLE_VALUE)
			CloseHandle(g_hConsoleOutput);
		if (g_hNewFile_tmp != INVALID_HANDLE_VALUE) {
			CloseHandle(g_hNewFile_tmp);
			DeleteFileA((LPCSTR)g_sNewFile_tmp);
		}
		WSACleanup();
		ExitProcess(TRUE);
	default:
		break;
	}

	return FALSE;
}

/*
 * Function description:
 * - Call to win32 API for getting user profile directory and add download directory.
 * Arguments:
 * - dd: Buffer for upload directory string path.
 */
void
build_download_directory(char dd[1024]) {
	DWORD szdd = 1024;

	ZeroMemory(dd, 1024);
	GetUserProfileDirectoryA(GetCurrentProcessToken(), dd, &szdd);
	strcat_s(dd, 1024, "\\Downloads\\");

	return;
}

/*
 * Function description:
 * - Write errors informations or informations in the console buffer. Before wu match the error
 *  message string.
 * Arguments:
 * - id: Message identificator that wu match in order to get the error string and related data.
 * - p: Data to insert in error string format (last function error code).
 */
void
write_info_in_console(enum idmsg id, void* p) {
	DWORD written;
	unsigned char i;
	char outConBuf[1024];

	for (i = 0; wumsg[i].id != INF_ERR_END; i++)
		if (wumsg[i].id == id)
			break;

	SetConsoleTextAttribute(g_hConsoleOutput, wumsg[i].wAtributes[0]);

	if (p != NULL) {
		ZeroMemory(outConBuf, 1024);
		StringCchPrintf(outConBuf, 1024, wumsg[i].Msg, p);
		WriteConsoleA(g_hConsoleOutput, outConBuf, (DWORD)strlen(outConBuf), &written, NULL);
	}
	else {
		WriteConsoleA(g_hConsoleOutput, wumsg[i].Msg, wumsg[i].szMsg, &written, NULL);
	}

	return;
};

/*
 * Function description:
 * - Clean pane rectangle for two cases. 1) Write progress bar download. 2) Empty incoming connections strings.
 * Arguments:
 * - cursorPosition: Cursor position where we start cleaning.
 */
void
clear_txrx_pane(COORD* cursorPosition) {
	DWORD written;
	COORD coordCR;

	for (coordCR.Y = cursorPosition[1].Y, coordCR.X = cursorPosition[1].X; coordCR.Y <= cursorPosition[1].Y + 6; coordCR.Y++) {
		SetConsoleCursorPosition(g_hConsoleOutput, coordCR);
		WriteConsoleA(g_hConsoleOutput, "                                                                                     ",
			sizeof("                                                                               ") - 1, &written, NULL);
	}

	cursorPosition->Y = (cursorPosition + 1)->Y;
	cursorPosition->X = (cursorPosition + 1)->X;
	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);

	return;
}

/*
 * Function description:
 * - Four for loop to draw one horizontale line, one vertical line, one horizontal line
 *   and one vertical line. These four line are a rectangle where wu will print connection
 *   info and upload progress bar.
 * Arguments:
 * - cursPosStart: Start cursor position where the rectangle begin.
 */
static void
draw_rectangle_in_console(COORD cursPosStart) {
	COORD cursPosEnd;
	DWORD written;

	cursPosStart.X = 1;

	SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);

	cursPosStart.X++;

	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	for (cursPosEnd.X = cursPosStart.X + 115, cursPosEnd.Y = cursPosStart.Y;
		cursPosStart.X <= cursPosEnd.X;
		cursPosStart.X++) {
		SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
		WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	}

	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);

	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	for (cursPosEnd.Y = cursPosStart.Y + 9, cursPosStart.Y++;
		cursPosStart.Y < cursPosEnd.Y;
		cursPosStart.Y++) {
		SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
		WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	}

	SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);

	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	for (cursPosEnd.X = cursPosStart.X - 117, cursPosEnd.Y = cursPosStart.Y;
		cursPosStart.X >= cursPosEnd.X;
		cursPosStart.X--) {
		SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
		WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	}

	cursPosStart.X++;
	cursPosStart.Y--;
	SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);

	SetConsoleTextAttribute(g_hConsoleOutput, COMMON_LVB_GRID_LVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	for (;
		cursPosStart.Y > cursPosEnd.Y - 9;
		cursPosStart.Y--) {
		SetConsoleCursorPosition(g_hConsoleOutput, cursPosStart);
		WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	}

	return;
}

static DWORD
wu_user_interface_part1(COORD cursorPosition[2], struct in_addr *inaddr) {
	CONSOLE_CURSOR_INFO cursorInfo;
	DWORD dwNumEntries;

	cursorPosition[0].X = 2;
	cursorPosition[0].Y = 2;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_PROMOTE_WIFIUPLOAD, NULL);

	cursorPosition[0].Y++;

	GetConsoleCursorInfo(g_hConsoleOutput, &cursorInfo);
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(g_hConsoleOutput, &cursorInfo);

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_SERVICE, NULL);

	cursorPosition[0].Y++;

	dwNumEntries = available_address_ui(cursorPosition, inaddr);

	cursorPosition[0].Y += 2;;
	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS, NULL);

	cursorPosition[0].X += 5;
	cursorPosition[0].Y += 2;
	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY, NULL);
	cursorPosition[0].X -= 5;

	return dwNumEntries;
}

/*
 * Function description:
 * - Alloc a new console, configure it. Initialize wisock, create listening socket. Initialize
 *   log files. Printf user interface, available address and so on. Enter main http loop and start
 *   to accept connection.
 * Arguments:
 * - hInstace: Not used.
 * - hPrevInstance: Not used.
 * - lpCmdLine: Not used.
 * - nCmdShow: Not used.
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
	DWORD written, read, ret;
	WSADATA wsaData;
	CHAR dd[1024];
	COORD cursorPosition[2];
	HWND consoleWindow;
	struct in_addr inaddr;
	unsigned char i;
	int s;
	char logentry[256];
	char logpath[512];
	char log_filename[sizeof("log_19700101.txt")];
	DWORD dwNumEntries;

	g_listensocket = g_usersocket = NULL;
	g_fplog = NULL;
	g_hNewFile_tmp = g_hConsoleOutput = INVALID_HANDLE_VALUE;
	inaddr.s_addr = 0;

	AllocConsole();

	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	SetConsoleTitleA(CONSOLE_TITLE);

#ifdef VERSION_FR
	SetConsoleCP(28591);
	SetConsoleOutputCP(28591);
	setlocale(LC_NUMERIC, "fr_FR");
#else
	setlocale(LC_NUMERIC, "en_US");
#endif

	consoleWindow = GetConsoleWindow();
	SetWindowLong(consoleWindow, GWL_STYLE, GetWindowLong(consoleWindow, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);

	g_hConsoleOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		CONSOLE_TEXTMODE_BUFFER,
		NULL);

	if (g_hConsoleOutput == INVALID_HANDLE_VALUE) {
		printf(ERR_FMT_MSG_INIT_CONSOLE, GetLastError());
		g_hConsoleOutput = INVALID_HANDLE_VALUE;
		_getch();
		ExitProcess(FALSE);
	}

	SetConsoleMode(g_hConsoleOutput, ENABLE_LVB_GRID_WORLDWIDE);

	ZeroMemory(&wsaData, sizeof(WSADATA));
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf(ERR_FMT_MSG_INIT_WINSOCK, GetLastError());
		_getch();
		ExitProcess(FALSE);
	}

	SetConsoleActiveScreenBuffer(g_hConsoleOutput);

	dwNumEntries = wu_user_interface_part1(cursorPosition, &inaddr);

	s = create_socket(&cursorPosition[0]);
	bind_socket(&cursorPosition[0], s, inaddr);

	cursorPosition[0].Y += (dwNumEntries < 3) ? 3 : 2;

	cursorPosition[1].Y = cursorPosition[0].Y;
	cursorPosition[1].X = cursorPosition[0].X;

	draw_rectangle_in_console(cursorPosition[0]);

	cursorPosition[0].Y = cursorPosition[1].Y;
	cursorPosition[0].X = cursorPosition[1].X;

	cursorPosition[0].Y++;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_UI_TX_RX, NULL);

	for (i = 0; wumsg[i].id != INF_WIFIUPLOAD_UI_TX_RX; i++)
		;
	cursorPosition[0].X += (USHORT)wumsg[i].szMsg;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);

	create_log_directory(logpath, log_filename);

	strcat_s(logpath, 512, "\\");
	strcat_s(logpath, 512, log_filename);

	if (fopen_s(&g_fplog, logpath, "a+")) {
		write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_FILE, logpath);
		WSACleanup();
		return 3;
	}

	for (cursorPosition[1].Y = cursorPosition[0].Y, cursorPosition[1].X = cursorPosition[0].X;;) {
		ret = http_loop(cursorPosition, &inaddr, s, logentry);

		fprintf(g_fplog, logentry);
		fflush(g_fplog);

		if (ret == 1)
			break;

		if (cursorPosition[0].Y >= cursorPosition[1].Y + 6)
			clear_txrx_pane(cursorPosition);
	}

	WSACleanup();
	return 0;
}

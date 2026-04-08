#include <Windows.h>
#include <WinSock.h>
#include <iphlpapi.h>
#include <UserEnv.h>
#include <strsafe.h>
#include <stdio.h>
#include <conio.h>
#include <direct.h>
#include <stdint.h>
#include <locale.h>
#include <sys\stat.h>

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_txstats.h"
#include "wu_http_nv.h"
#include "wu_tls_conn.h"
#include "wu_tls_main.h"
#include "wu_msg.h"
#include "wu_main.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_log.h"
#include "wu_socket.h"
#include "wu_http_loop.h"
#include "wu_available_address.h"


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "Ncrypt.lib")

extern struct wu_msg wumsg[];
FILE* g_fplog;
FILE* g_fphttpslog;
SOCKET g_listensocket = 0;
SOCKET g_listenhttpssocket = 0;
SOCKET g_usersocket = 0;
SOCKET g_tls_sclt = 0;
HANDLE g_hConsoleOutput;
HANDLE g_hNewFile_tmp;
unsigned char g_sNewFile_tmp[1024];
int g_tls_firstsend = 0;
CtxtHandle *g_ctxtHandle = NULL;
CredHandle *g_credHandle = NULL;
extern char* encryptBuffer;

static DWORD wu_user_interface_part1(COORD cursorPosition[2], struct in_addr* inaddr);
static void draw_rectangle_in_console(COORD cursPosStart);

BOOL WINAPI
HandlerRoutine(_In_ DWORD dwCtrlType)
{
	switch (dwCtrlType) {
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (g_listensocket)
			closesocket(g_listensocket);
		if (g_usersocket)
			closesocket(g_usersocket);
		if (g_listenhttpssocket)
			closesocket(g_listenhttpssocket);
		if (g_hConsoleOutput != INVALID_HANDLE_VALUE)
			CloseHandle(g_hConsoleOutput);
		if (g_hNewFile_tmp != INVALID_HANDLE_VALUE) {
			CloseHandle(g_hNewFile_tmp);
			DeleteFileA((LPCSTR)g_sNewFile_tmp);
		}
		if (g_credHandle != NULL && g_ctxtHandle != NULL && g_tls_sclt) {
			tls_shutdown(g_ctxtHandle, g_credHandle, g_tls_sclt);
			DeleteSecurityContext(g_ctxtHandle);
			FreeCredentialHandle(g_credHandle);
		}
        if (g_fplog != NULL)
            fclose(g_fplog);
        if (g_fphttpslog != NULL)
            fclose(g_fphttpslog);
        if (encryptBuffer != NULL)
            free(encryptBuffer);
        WSACleanup();
        return TRUE; /* indicate we've handled the control event */
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
 * - Write in console information or error message.
 * Arguments:
 * - id: Id of message to write. See wu_msg.h for more details.
 * - p: Pointer to data used in message formatting. See wu_msg.h for more details.
 * - err: Error code used in message formatting. See wu_msg.h for more details.
 */

void
write_info_in_console(enum idmsg id, void* p, DWORD err) {
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
	else if (err != 0) {
		ZeroMemory(outConBuf, 1024);
		StringCchPrintf(outConBuf, 1024, wumsg[i].Msg, err);
		WriteConsoleA(g_hConsoleOutput, outConBuf, (DWORD)strlen(outConBuf), &written, NULL);
	}
	else {
		WriteConsoleA(g_hConsoleOutput, wumsg[i].Msg, wumsg[i].szMsg, &written, NULL);
	}

	return;
}

/*
 * Function description:
 * - Show error message in console and wait user to close the program.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes info or errors.
 * - id: Id of message to write. See wu_msg.h for more details.
 * - p: Pointer to data used in message formatting. See wu_msg.h for more details.
 * - err: Error code used in message formatting. See wu_msg.h for more details.
 */

void
show_error_wait_close(COORD *cursorPosition, enum idmsg id, const void* p, DWORD err) {
	/* Show text error in console */
	cursorPosition->Y++;
	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
	write_info_in_console(id, (void*)p, err);

	/* Wait user close */
	for (;;) Sleep(10000);

	return;
}

/*
 * Function description:
 * - Clear tx/rx pane in console. Tx/rx pane is the rectangle bottom part of
 *   console where wu writes information about upload/download transfert.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes info or errors.
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
 * - Write software information and wifiupload start information in console.
 *   Also get available address and write them in console.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes info.
 * - inaddr: IPv4 address wifiupload will listen to.
 * Return value:
 * - Number of available address. (not used)
 */

static DWORD
wu_user_interface_part1(COORD cursorPosition[2], struct in_addr* inaddr) {
	CONSOLE_CURSOR_INFO cursorInfo;
	DWORD dwNumEntries;

	cursorPosition[0].X = 2;
	cursorPosition[0].Y = 2;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_PROMOTE_WIFIUPLOAD, NULL, 0);

	cursorPosition[0].Y++;

	GetConsoleCursorInfo(g_hConsoleOutput, &cursorInfo);
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(g_hConsoleOutput, &cursorInfo);

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_SERVICE, NULL, 0);

	cursorPosition[0].Y++;

	dwNumEntries = available_address_ui(cursorPosition, inaddr);

	cursorPosition[0].Y += 2;;
	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS, NULL, 0);

	cursorPosition[0].X += 5;
	cursorPosition[0].Y += 2;
	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY, NULL, 0);
	cursorPosition[0].X -= 5;

	return dwNumEntries;
}

/*
 * Function description:
 * - Draw a rectangle on the bottom part of console. Upload/download information and
 *   progress bar are written in this rectangle.
 * Arguments:
 * - cursPosStart: Console cursor position where rectangle left upper start.
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
	DWORD ret;
	WSADATA wsaData;
	COORD cursorPosition[2];
	HWND consoleWindow;
	HANDLE hThread1;
	struct paramThread prThread;
	struct in_addr inaddr;
	unsigned char i;
	SOCKET s;
	char logentry[256];
	char logpath[512];
	char logpath_https[512];
	char log_filename[sizeof("log_19700101.txt")];
	char loghttps_filename[sizeof("loghttps_19700101.txt")];
	DWORD dwNumEntries;

	g_fphttpslog = g_fplog = NULL;
	g_hNewFile_tmp = g_hConsoleOutput = INVALID_HANDLE_VALUE;
	g_tls_firstsend = 1;
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
	write_info_in_console(INF_WIFIUPLOAD_UI_TX_RX, NULL, 0);

	for (i = 0; wumsg[i].id != INF_WIFIUPLOAD_UI_TX_RX; i++)
		;
	cursorPosition[0].X += (USHORT)wumsg[i].szMsg;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);

	create_log_directory(logpath, log_filename, loghttps_filename);

	ZeroMemory(logpath_https, 512);
	strcpy_s(logpath_https, 512, logpath);

	strcat_s(logpath, 512, "\\");
	strcat_s(logpath, 512, log_filename);

	strcat_s(logpath_https, 512, "\\");
	strcat_s(logpath_https, 512, loghttps_filename);

	if (fopen_s(&g_fplog, logpath, "a+")) {
		write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_FILE, logpath, 0);
		for (;;) Sleep(10000);
	}

	if (fopen_s(&g_fphttpslog, logpath_https, "a+")) {
		write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_FILE, logpath_https, 0);
		for (;;) Sleep(10000);
	}

	ZeroMemory(&prThread, sizeof(struct paramThread));
	memcpy_s(&prThread.inaddr, sizeof(struct in_addr), &inaddr, sizeof(struct in_addr));
	prThread.cursorPosition[0] = cursorPosition[0];
	prThread.cursorPosition[1] = cursorPosition[0];
	hThread1 = CreateThread(NULL, 0, wu_tls_loop, &prThread, 0, NULL);

	for (cursorPosition[1].Y = cursorPosition[0].Y, cursorPosition[1].X = cursorPosition[0].X;;) {
		ret = http_loop(cursorPosition, &inaddr, s, logentry);

		fprintf(g_fplog, "%s", logentry);
		fflush(g_fplog);

		if (ret == 1)
			break;

		if (cursorPosition[0].Y >= cursorPosition[1].Y + 6)
			clear_txrx_pane(cursorPosition);
	}

	WSACleanup();
	return 0;
}

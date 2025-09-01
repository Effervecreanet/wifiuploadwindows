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


#include "wu_x509_main.h"
#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_loop.h"


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
FILE *g_fplog;
FILE *g_fphttpslog;
int *g_listensocket;
int *g_listenhttpssocket;
int *g_usersocket;
HANDLE g_hConsoleOutput;
HANDLE g_hNewFile_tmp;
unsigned char g_sNewFile_tmp[1024];

BOOL WINAPI
HandlerRoutine(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
		if (g_fplog != NULL)
			fclose(g_fplog);
		if (g_fphttpslog != NULL)
			fclose(g_fphttpslog);
		if (g_usersocket != NULL && *g_usersocket != 0)
			closesocket(*g_usersocket);
		if (g_listensocket != NULL)
			closesocket(*g_listensocket);
		if (g_hConsoleOutput != INVALID_HANDLE_VALUE)
			CloseHandle(g_hConsoleOutput);
		if (g_hNewFile_tmp != INVALID_HANDLE_VALUE){
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

void
create_download_directory(char dd[1024]) {
    DWORD szdd = 1024;

    ZeroMemory(dd, 1024);
    GetUserProfileDirectoryA(GetCurrentProcessToken(), dd, &szdd);
    strcat_s(dd, 1024, "\\Downloads\\");

    return;
}

void
write_info_in_console(enum idmsg id, void *p, DWORD err) {
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
    } else if (err != 0) {
        ZeroMemory(outConBuf, 1024);
        StringCchPrintf(outConBuf, 1024, wumsg[i].Msg, err);
        WriteConsoleA(g_hConsoleOutput, outConBuf, (DWORD)strlen(outConBuf), &written, NULL);
	} else {
        WriteConsoleA(g_hConsoleOutput, wumsg[i].Msg, wumsg[i].szMsg, &written, NULL);
    }

    return;
};

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

int main(void)
{
    DWORD written, read, ret;
    WSADATA wsaData;
    MIB_IPADDRTABLE ipAddrTable[4];
    ULONG sizeIpAddrTable = sizeof(MIB_IPADDRTABLE) * 4;
    CONSOLE_CURSOR_INFO cursorInfo;
    CHAR dd[1024];
    COORD cursorPosition[2];
    HWND consoleWindow;
	HANDLE hThread1;
	struct paramThread prThread;
    struct in_addr inaddr;
    unsigned char i;
    int s;
    struct _stat statbuff;
    char logentry[256];
    SYSTEMTIME systime;
    char logpath[512];
    char loghttps_path[512];
    char log_filename[sizeof("log_19700101.txt")];
    char httpslog_filename[sizeof("loghttps_19700101.txt")];
	char userprofile[255 + sizeof(LOG_DIRECTORY)];

	g_listensocket = g_usersocket = NULL;
	g_fphttpslog = g_fplog = NULL;
	g_hNewFile_tmp = g_hConsoleOutput = INVALID_HANDLE_VALUE;
	inaddr.s_addr = 0;

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

    ZeroMemory(&ipAddrTable, sizeIpAddrTable);    
    ret = GetIpAddrTable((PMIB_IPADDRTABLE)&ipAddrTable, &sizeIpAddrTable, TRUE);

    if (ret != NO_ERROR || ipAddrTable[0].dwNumEntries < 2) {
        INPUT_RECORD inRec;

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(ERR_MSG_CONNECTIVITY, NULL, 0);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

    } 
    else if (ipAddrTable[0].dwNumEntries == 2) {
        PCHAR pStringIpAddr;

        ZeroMemory(&inaddr, sizeof(struct in_addr));
        CopyMemory(&inaddr, &(ipAddrTable->table[1].dwAddr), sizeof(DWORD));

        pStringIpAddr = inet_ntoa(inaddr);

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_ONE_AVAILABLE_ADDR, NULL, 0);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_ONE_AVAILABLE_ADDR_2, pStringIpAddr, 0);
        cursorPosition[0].X -= 5;

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_WIFIUPLOAD_IS_LISTENING_TO, NULL, 0);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr), 0);

        SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        cursorPosition[0].X -= 5;
    } 
    else if (ipAddrTable[0].dwNumEntries == 3) {
        struct in_addr inaddr1;
        PCHAR pStringIpAddr1;
        CHAR strIpAddrAvail1[255];
        struct in_addr inaddr2;
        PCHAR pStringIpAddr2;
        CHAR strIpAddrAvail2[255];
        INPUT_RECORD inRec;
        DWORD dwUsrChoice;
        PMIB_IPADDRROW addrRow = ipAddrTable->table;


        ZeroMemory(&inaddr1, sizeof(struct in_addr));
        CopyMemory(&inaddr1, &((addrRow + 1)->dwAddr), sizeof(DWORD));
        ZeroMemory(&inaddr2, sizeof(struct in_addr));
        CopyMemory(&inaddr2, &((addrRow + 2)->dwAddr), sizeof(DWORD));

        ZeroMemory(strIpAddrAvail1, 255);
        ZeroMemory(strIpAddrAvail2, 255);

        cursorPosition[0].Y += 2;

        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_MSG_TWO_AVAILABLE_ADDR, NULL, 0);

        cursorPosition[0].X += 4;
        cursorPosition[0].Y += 2;

        pStringIpAddr1 = inet_ntoa(inaddr1);
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1, pStringIpAddr1, 0);
        cursorPosition[0].Y++;
        pStringIpAddr2 = inet_ntoa(inaddr2);
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2, pStringIpAddr2, 0);
        cursorPosition[0].Y++;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_MSG_CHOICE_QUESTION, NULL, 0);
        
        do {
            ZeroMemory(&inRec, sizeof(INPUT_RECORD));
            while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read)) {
                if (inRec.Event.KeyEvent.bKeyDown != TRUE)
                    continue;
                else
                    break;
            }
            dwUsrChoice = atoi((const char*)&inRec.Event.KeyEvent.uChar.AsciiChar);
        } while (dwUsrChoice >= ipAddrTable->dwNumEntries || dwUsrChoice == 0);

        cursorPosition[0].X += sizeof(INF_MSG_CHOICE_QUESTION) + 5;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        WriteConsoleA(g_hConsoleOutput, &inRec.Event.KeyEvent.uChar.AsciiChar, 1, &written, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X -= sizeof(INF_MSG_CHOICE_QUESTION) + 5;

        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_WIFIUPLOAD_IS_LISTENING_TO, NULL, 0);

        cursorPosition[0].Y++;

        if (inRec.Event.KeyEvent.uChar.AsciiChar == '1') {
            ZeroMemory(&inaddr, sizeof(struct in_addr));
            CopyMemory(&inaddr, &(addrRow + 1)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
            write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr1), 0);
            SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            cursorPosition[0].X -= 5;
        }
        else {
            ZeroMemory(&inaddr, sizeof(struct in_addr));
            CopyMemory(&inaddr, &(addrRow + 2)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
            write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr2), 0);
            cursorPosition[0].X -= 5;
        }
    }
    else {
        INPUT_RECORD inRec;
		cursorPosition[0].Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
		write_info_in_console(ERR_MSG_TOO_MANY_ADDR, NULL, 0);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

		ExitProcess(4);
    }

    cursorPosition[0].Y += 2;;
    SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
    write_info_in_console(INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS, NULL, 0);

    ZeroMemory(dd, 1024);
    create_download_directory(dd);

    cursorPosition[0].X += 5;
    cursorPosition[0].Y += 2;
    SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
    write_info_in_console(INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY, NULL, 0);
    cursorPosition[0].X -= 5;

    s = create_socket(&cursorPosition[0]);
    bind_socket(&cursorPosition[0], s, inaddr);

    cursorPosition[0].Y += ((ipAddrTable->dwNumEntries < 3) ? 3 : 2);

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

	ZeroMemory(logpath, 512);
	ZeroMemory(userprofile, 255 + sizeof(LOG_DIRECTORY));
	getenv_s((size_t*)&ret, userprofile, (size_t)(255 + sizeof(LOG_DIRECTORY)), "USERPROFILE");
	sprintf_s(logpath, 255 + 1 + sizeof(LOG_DIRECTORY), "%s\\%s", userprofile, LOG_DIRECTORY);
    ret = _stat(logpath, &statbuff);
    if (ret && _mkdir(logpath)) {
			INPUT_RECORD inRec;
			write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, "logs", 0);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	} else {
        	char wYearStr[5];
	
			GetSystemTime(&systime);
			ZeroMemory(log_filename, sizeof("log_19700101.txt"));
			ZeroMemory(wYearStr, 5);
			sprintf_s(wYearStr, 5, "%i", systime.wYear);
			strcat_s(logpath, 512, "\\");
			strcat_s(logpath, 512, wYearStr);
		
		if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
			INPUT_RECORD inRec;
			write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath, 0);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		} else {
			char wMonthStr[3];
			
			if (systime.wMonth < 10) {
				ZeroMemory(wMonthStr, 3);
				sprintf_s(wMonthStr, 3, "0%i", systime.wMonth);
				strcat_s(logpath, 512, "\\");
				strcat_s(logpath, 512, wMonthStr);
			} else {
				ZeroMemory(wMonthStr, 3);
				sprintf_s(wMonthStr, 3, "%i", systime.wMonth);
				strcat_s(logpath, 512, "\\");
				strcat_s(logpath, 512, wMonthStr);
			}
			if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
				INPUT_RECORD inRec;
				write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath, 0);
				while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			} else {
				char wDayStr[3];

				if (systime.wMonth < 10) {
					if (systime.wDay < 10) {
						sprintf_s(wDayStr, 3, "0%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i0%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						ZeroMemory(httpslog_filename, sizeof("loghttps_19700101.txt"));
						sprintf_s(httpslog_filename, sizeof("loghttps_19700101.txt"), "loghttps_%i0%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					} else {
						sprintf_s(wDayStr, 3, "%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i0%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						ZeroMemory(httpslog_filename, sizeof("loghttps_19700101.txt"));
						sprintf_s(httpslog_filename, sizeof("loghttps_19700101.txt"), "loghttps_%i0%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					}
				} else if (systime.wDay < 10) {
						sprintf_s(wDayStr, 3, "0%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						ZeroMemory(httpslog_filename, sizeof("loghttps_19700101.txt"));
						sprintf_s(httpslog_filename, sizeof("loghttps_19700101.txt"), "loghttps_%i%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
				} else {
						sprintf_s(wDayStr, 3, "%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						ZeroMemory(httpslog_filename, sizeof("loghttps_19700101.txt"));
						sprintf_s(httpslog_filename, sizeof("loghttps_19700101.txt"), "loghttps_%i%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
				}

				if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
					INPUT_RECORD inRec;
					write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath, 0);
					while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
					}
				}
			}
	}		

    strcat_s(logpath, 512, "\\");
	strcpy(loghttps_path, logpath);
    strcat_s(logpath, 512, log_filename);
	strcat_s(loghttps_path, 512, httpslog_filename);

    if (fopen_s(&g_fplog, logpath, "a+")) {
		write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_FILE, logpath, 0);
		WSACleanup();
		return 3;
    } else if (fopen_s(&g_fphttpslog, loghttps_path, "a+")) {
		write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_FILE, loghttps_path, 0);
		WSACleanup();
		return 3;
	}

	ZeroMemory(&prThread, sizeof(struct paramThread));
	memcpy_s(&prThread.inaddr, sizeof(struct in_addr), &inaddr, sizeof(struct in_addr));
	prThread.cursorPosition = cursorPosition[0];
	hThread1 = CreateThread(NULL, 0, wu_x509_func, &prThread, 0, NULL);

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

#include <Windows.h>
#include <WinSock.h>
#include <iphlpapi.h>
#include <UserEnv.h>
#include <strsafe.h>
#include <stdio.h>
#include <locale.h>
#include <sys\stat.h>


#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "userenv.lib")

extern struct wu_msg wumsg[];
FILE *fp_log;

BOOL WINAPI
HandlerRoutine(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
		fclose(fp_log);
        WSACleanup();
        ExitProcess(TRUE);
    default:
        break;
    }

    return FALSE;
}

int
create_download_directory(unsigned char dd[1024]) {
    DWORD szdd = 1024;

    ZeroMemory(dd, 1024);
    GetUserProfileDirectoryA(GetCurrentProcessToken(), dd, &szdd);
    strcat_s(dd, 1024, "\\Downloads\\");

    return 1;
}

void
WriteConsoleA_INFO(HANDLE conScreenBuffer, enum idmsg id, void *p) {
    DWORD written;
    unsigned char i;
    char outConBuf[1024];

    for (i = 0; wumsg[i].id != INF_ERR_END; i++)
        if (wumsg[i].id == id)
            break;
    
    SetConsoleTextAttribute(conScreenBuffer, wumsg[i].wAtributes[0]);

    if (p != NULL) {
        ZeroMemory(outConBuf, 1024);
        StringCchPrintf(outConBuf, 1024, wumsg[i].Msg, p);
        WriteConsoleA(conScreenBuffer, outConBuf, (DWORD)strlen(outConBuf), &written, NULL);
    }
    else {
        WriteConsoleA(conScreenBuffer, wumsg[i].Msg, wumsg[i].szMsg, &written, NULL);
    }

    return;
};

void
clearTXRXPane(HANDLE conScreenBuffer, COORD* cursorPosition) {
    DWORD written;
    COORD coordCR;

    for (coordCR.Y = cursorPosition[1].Y, coordCR.X = cursorPosition[1].X; coordCR.Y <= cursorPosition[1].Y + 6; coordCR.Y++) {
        SetConsoleCursorPosition(conScreenBuffer, coordCR);
        WriteConsoleA(conScreenBuffer, "                                                                                     ",
            sizeof("                                                                               ") - 1, &written, NULL);
    }

    cursorPosition->Y = (cursorPosition + 1)->Y;
    cursorPosition->X = (cursorPosition + 1)->X;
    SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);

    return;
}

static void
consDrawRect(HANDLE hConScreenBuf, COORD cursPosStart) {
    COORD cursPosEnd;
    DWORD written;

    cursPosStart.X = 1;

    SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);
    
    cursPosStart.X++;
    
    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    for (cursPosEnd.X = cursPosStart.X + 115, cursPosEnd.Y = cursPosStart.Y;
        cursPosStart.X <= cursPosEnd.X;
        cursPosStart.X++) {
        SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
        WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);
    }

    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);

    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    for (cursPosEnd.Y = cursPosStart.Y + 9, cursPosStart.Y++;
        cursPosStart.Y < cursPosEnd.Y;
        cursPosStart.Y++) {
        SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
        WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);
    }

    SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_RVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);

    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    for (cursPosEnd.X = cursPosStart.X - 117, cursPosEnd.Y = cursPosStart.Y;
        cursPosStart.X >= cursPosEnd.X;
        cursPosStart.X--) {
        SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
        WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);
    }

    cursPosStart.X++;
    cursPosStart.Y--;
    SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);

    SetConsoleTextAttribute(hConScreenBuf, COMMON_LVB_GRID_LVERTICAL | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    for (;
        cursPosStart.Y > cursPosEnd.Y - 9;
        cursPosStart.Y--) {
        SetConsoleCursorPosition(hConScreenBuf, cursPosStart);
        WriteConsoleA(hConScreenBuf, " ", 1, &written, NULL);
    }

    return;
}

int main(void)
{
    HANDLE conScreenBuffer;
    DWORD written, read, ret;
    WSADATA wsaData;
    MIB_IPADDRTABLE ipAddrTable[4];
    ULONG sizeIpAddrTable = sizeof(MIB_IPADDRTABLE) * 4;
    CONSOLE_CURSOR_INFO cursorInfo;
    CHAR dd[1024];
    COORD cursorPosition[2];
    HWND consoleWindow;
    struct in_addr inaddr;
    unsigned char i;
    int s;
    struct _stat statbuff;
    char logentry[256];
    SYSTEMTIME systime;
    char errMsgFormatStr[127];
    char logpath[512];
    char log_filename[sizeof("log_19700101.txt")];

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

    conScreenBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);

    if (conScreenBuffer == INVALID_HANDLE_VALUE) {
        printf(ERR_FMT_MSG_INIT_CONSOLE, GetLastError());
        while (1)
            Sleep(1000);

        ExitProcess(1);
    }

    SetConsoleMode(conScreenBuffer, ENABLE_LVB_GRID_WORLDWIDE);

    ZeroMemory(&wsaData, sizeof(WSADATA));
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        printf(ERR_FMT_MSG_INIT_WINSOCK, GetLastError());
        while (1)
            Sleep(1000);

        ExitProcess(2);
    }

    SetConsoleActiveScreenBuffer(conScreenBuffer);

    cursorPosition[0].X = 2;
    cursorPosition[0].Y = 2;

    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, INF_PROMOTE_WIFIUPLOAD, NULL);

    cursorPosition[0].Y++;

    GetConsoleCursorInfo(conScreenBuffer, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(conScreenBuffer, &cursorInfo);

    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_SERVICE, NULL);

    cursorPosition[0].Y++;

    ZeroMemory(&ipAddrTable, sizeIpAddrTable);    
    ret = GetIpAddrTable((PMIB_IPADDRTABLE)&ipAddrTable, &sizeIpAddrTable, TRUE);

    if (ret != NO_ERROR || ipAddrTable[0].dwNumEntries < 2) {
        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CONNECTIVITY, NULL);

        while (1)
            Sleep(1000);

        ExitProcess(3);
    } 
    else if (ipAddrTable[0].dwNumEntries == 2) {
        PCHAR pStringIpAddr;

        ZeroMemory(&inaddr, sizeof(struct in_addr));
        CopyMemory(&inaddr, &(ipAddrTable->table[1].dwAddr), sizeof(DWORD));

        pStringIpAddr = inet_ntoa(inaddr);

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_FMT_MSG_ONE_AVAILABLE_ADDR, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_FMT_MSG_ONE_AVAILABLE_ADDR_2, pStringIpAddr);
        cursorPosition[0].X -= 5;

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_IS_LISTENING_TO, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr));

        SetConsoleTextAttribute(conScreenBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
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

        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_MSG_TWO_AVAILABLE_ADDR, NULL);

        cursorPosition[0].X += 4;
        cursorPosition[0].Y += 2;

        pStringIpAddr1 = inet_ntoa(inaddr1);
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1, pStringIpAddr1);
        cursorPosition[0].Y++;
        pStringIpAddr2 = inet_ntoa(inaddr2);
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2, pStringIpAddr2);
        cursorPosition[0].Y++;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_MSG_CHOICE_QUESTION, NULL);
        
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

        cursorPosition[0].X += sizeof(INF_MSG_CHOICE_QUESTION) - 1;
        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA(conScreenBuffer, &inRec.Event.KeyEvent.uChar.AsciiChar, 1, &written, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X -= sizeof(INF_MSG_CHOICE_QUESTION) - 1;

        SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
        WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_IS_LISTENING_TO, NULL);

        cursorPosition[0].Y++;

        if (inRec.Event.KeyEvent.uChar.AsciiChar == '1') {
            ZeroMemory(&inaddr, sizeof(struct in_addr));
            CopyMemory(&inaddr, &(addrRow + 1)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
            WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr1));
            SetConsoleTextAttribute(conScreenBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            cursorPosition[0].X -= 5;
        }
        else {
            ZeroMemory(&inaddr, sizeof(struct in_addr));
            CopyMemory(&inaddr, &(addrRow + 2)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
            WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr2));
            cursorPosition[0].X -= 5;
        }
    }
    else {
    cursorPosition[0].Y++;
    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_TOO_MANY_ADDR, NULL);

    while (1)
        Sleep(1000);

    ExitProcess(4);
    }

    cursorPosition[0].Y += 2;;
    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS, NULL);

    ZeroMemory(dd, 1024);
    create_download_directory(dd);

    cursorPosition[0].X += 5;
    cursorPosition[0].Y += 2;
    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY, NULL);
    cursorPosition[0].X -= 5;

    s = create_socket(conScreenBuffer, &cursorPosition[0]);
    bind_socket(conScreenBuffer, &cursorPosition[0], s, inaddr);

    cursorPosition[0].Y += ((ipAddrTable->dwNumEntries < 3) ? 3 : 2);

    cursorPosition[1].Y = cursorPosition[0].Y;
    cursorPosition[1].X = cursorPosition[0].X;
    consDrawRect(conScreenBuffer, cursorPosition[0]);
    cursorPosition[0].Y = cursorPosition[1].Y;
    cursorPosition[0].X = cursorPosition[1].X;

    cursorPosition[0].Y++;

    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);
    WriteConsoleA_INFO(conScreenBuffer, INF_WIFIUPLOAD_UI_TX_RX, NULL);
 
    for (i = 0; wumsg[i].id != INF_WIFIUPLOAD_UI_TX_RX; i++)
        ;
    cursorPosition[0].X += (USHORT)wumsg[i].szMsg;
    
    SetConsoleCursorPosition(conScreenBuffer, cursorPosition[0]);

	ZeroMemory(logpath, 512);
	sprintf(logpath, "%s\\%s", getenv("USERPROFILE"), LOG_DIRECTORY);
    ret = _stat(logpath, &statbuff);
    if (ret) {
	    if (_mkdir(logpath)) {
		INPUT_RECORD inRec;
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, "logs");
		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read)) {
			if (inRec.Event.KeyEvent.bKeyDown != TRUE)
			    continue;
		        WSACleanup();
		        return 2;
		    }
	    } else {
logyear:	INPUT_RECORD inRec;
			char wYearStr[5];

		GetSystemTime(&systime);
		ZeroMemory(log_filename, sizeof("log_19700101.txt"));
			ZeroMemory(wYearStr, 5);
			sprintf(wYearStr, "%i", systime.wYear);
			strcat(logpath, "\\");
			strcat(logpath, wYearStr);
		
		if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
			INPUT_RECORD inRec;
			WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read)) {
				if (inRec.Event.KeyEvent.bKeyDown != TRUE)
				    continue;
				WSACleanup();
				return 2;
		    }
		} else {
			char wMonthStr[3];
			
			if (systime.wMonth < 10) {
				ZeroMemory(wMonthStr, 3);
				sprintf(wMonthStr, "0%i", systime.wMonth);
				strcat(logpath, "\\");
				strcat(logpath, wMonthStr);
			} else {
				ZeroMemory(wMonthStr, 3);
				sprintf(wMonthStr, "%i", systime.wMonth);
				strcat(logpath, "\\");
				strcat(logpath, wMonthStr);
			}
			if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
				INPUT_RECORD inRec;
				WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
				while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read)) {
					if (inRec.Event.KeyEvent.bKeyDown != TRUE)
					    continue;
				WSACleanup();
				return 2;
			        }
			} else {
				char wDayStr[3];

				if (systime.wMonth < 10) {
					if (systime.wDay < 10) {
						sprintf(wDayStr, "0%i", systime.wDay);
						strcat(logpath, "\\");
						strcat(logpath, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf(log_filename, "log_%i0%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					} else {
						sprintf(wDayStr, "%i", systime.wDay);
						strcat(logpath, "\\");
						strcat(logpath, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf(log_filename, "log_%i0%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					}
				} else if (systime.wDay < 10) {
						sprintf(wDayStr, "0%i", systime.wDay);
						strcat(logpath, "\\");
						strcat(logpath, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf(log_filename, "log_%i%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
				} else {
						sprintf(wDayStr, "%i", systime.wDay);
						strcat(logpath, "\\");
						strcat(logpath, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf(log_filename, "log_%i%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
				}

				if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
					INPUT_RECORD inRec;
					WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
					while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read)) {
						if (inRec.Event.KeyEvent.bKeyDown != TRUE)
						    continue;
					WSACleanup();
					return 2;
					}
				}
			}
		}		
	}
    } else {
	    goto logyear;
    }

    strcat(logpath, "\\");
    strcat(logpath, log_filename);

    fp_log = fopen(logpath, "a+");
    if (!fp_log) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_CREATE_LOG_FILE, logpath);
		WSACleanup();
		return 3;
    }

    for (cursorPosition[1].Y = cursorPosition[0].Y, cursorPosition[1].X = cursorPosition[0].X;;) {
        ret = http_loop(conScreenBuffer, cursorPosition, s, logentry);

		fprintf(fp_log, logentry);
		fflush(fp_log);

		if (ret == 1)
			break;

        if (cursorPosition[0].Y >= cursorPosition[1].Y + 6)
            clearTXRXPane(conScreenBuffer, cursorPosition);
    }

    WSACleanup();
    return 0;
}

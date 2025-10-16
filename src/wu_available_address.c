#include <Windows.h>
#include <iphlpapi.h>

#include "wu_msg.h"

extern HANDLE g_hConsoleOutput;

void
write_info_one_available_addr(COORD cursorPosition[2], struct in_addr *inaddr, MIB_IPADDRTABLE ipAddrTable[4]) {
        PCHAR pStringIpAddr;

        ZeroMemory(inaddr, sizeof(struct in_addr));
        CopyMemory(inaddr, &(ipAddrTable->table[1].dwAddr), sizeof(DWORD));

        pStringIpAddr = inet_ntoa(*inaddr);

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_ONE_AVAILABLE_ADDR, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_ONE_AVAILABLE_ADDR_2, pStringIpAddr);
        cursorPosition[0].X -= 5;

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_WIFIUPLOAD_IS_LISTENING_TO, NULL);

        cursorPosition[0].Y += 2;
        cursorPosition[0].X += 5;
        
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(*inaddr));

        SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        cursorPosition[0].X -= 5;
		
		return;
} 

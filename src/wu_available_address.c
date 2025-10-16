#include <Windows.h>
#include <iphlpapi.h>

#include "wu_msg.h"

extern HANDLE g_hConsoleOutput;

static void
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

static void
ui_two_available_addr(COORD cursorPosition[2], struct in_addr *inaddr, MIB_IPADDRTABLE ipAddrTable[4]) {
        struct in_addr inaddr1;
        PCHAR pStringIpAddr1;
        CHAR strIpAddrAvail1[255];
        struct in_addr inaddr2;
        PCHAR pStringIpAddr2;
        CHAR strIpAddrAvail2[255];
        INPUT_RECORD inRec;
        DWORD dwUsrChoice;
        PMIB_IPADDRROW addrRow = ipAddrTable->table;
		DWORD read, written;


        ZeroMemory(&inaddr1, sizeof(struct in_addr));
        CopyMemory(&inaddr1, &((addrRow + 1)->dwAddr), sizeof(DWORD));
        ZeroMemory(&inaddr2, sizeof(struct in_addr));
        CopyMemory(&inaddr2, &((addrRow + 2)->dwAddr), sizeof(DWORD));

        ZeroMemory(strIpAddrAvail1, 255);
        ZeroMemory(strIpAddrAvail2, 255);

        cursorPosition[0].Y += 2;

        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_MSG_TWO_AVAILABLE_ADDR, NULL);

        cursorPosition[0].X += 4;
        cursorPosition[0].Y += 2;

        pStringIpAddr1 = inet_ntoa(inaddr1);
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1, pStringIpAddr1);
        cursorPosition[0].Y++;
        pStringIpAddr2 = inet_ntoa(inaddr2);
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2, pStringIpAddr2);
        cursorPosition[0].Y++;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(INF_MSG_CHOICE_QUESTION, NULL);
        
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
        write_info_in_console(INF_WIFIUPLOAD_IS_LISTENING_TO, NULL);

        cursorPosition[0].Y++;

        if (inRec.Event.KeyEvent.uChar.AsciiChar == '1') {
            ZeroMemory(inaddr, sizeof(struct in_addr));
            CopyMemory(inaddr, &(addrRow + 1)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
            write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr1));
            SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            cursorPosition[0].X -= 5;
        }
        else {
            ZeroMemory(inaddr, sizeof(struct in_addr));
            CopyMemory(inaddr, &(addrRow + 2)->dwAddr, sizeof(DWORD));

            cursorPosition[0].X += 5;
            cursorPosition[0].Y++;
            SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
            write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr2));
            cursorPosition[0].X -= 5;
        }

	return;
}


void available_address_ui(COORD cursorPosition[2], struct in_addr *inaddr) {
    MIB_IPADDRTABLE ipAddrTable[4];
    ULONG sizeIpAddrTable = sizeof(MIB_IPADDRTABLE) * 4;
	int ret;

    ZeroMemory(&ipAddrTable, sizeIpAddrTable);    
    ret = GetIpAddrTable((PMIB_IPADDRTABLE)&ipAddrTable, &sizeIpAddrTable, TRUE);

    if (ret != NO_ERROR || ipAddrTable[0].dwNumEntries < 2) {
        INPUT_RECORD inRec;
		DWORD read;

        cursorPosition[0].Y += 2;
        SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
        write_info_in_console(ERR_MSG_CONNECTIVITY, NULL);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

    } 
    else if (ipAddrTable[0].dwNumEntries == 2) {
		write_info_one_available_addr(cursorPosition, inaddr, ipAddrTable);
    } 
    else if (ipAddrTable[0].dwNumEntries == 3) {
		ui_two_available_addr(cursorPosition, inaddr, ipAddrTable);
    }
    else {
        INPUT_RECORD inRec;
		cursorPosition[0].Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
		write_info_in_console(ERR_MSG_TOO_MANY_ADDR, NULL);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

		ExitProcess(4);
    }

	return;
}

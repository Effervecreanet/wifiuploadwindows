#include <Windows.h>
#include <iphlpapi.h>

#include "wu_msg.h"

extern HANDLE g_hConsoleOutput;

static void write_info_one_available_addr(COORD cursorPosition[2], struct in_addr* inaddr, MIB_IPADDRTABLE ipAddrTable[4]);
static void ui_two_available_addr(COORD cursorPosition[2], struct in_addr* inaddr, MIB_IPADDRTABLE ipAddrTable[4]);

/*
 * Function description;
 * - If only one IP address is available for the server socket wu print user
 *   interface message and welcome the user to connect his phone to this
 *   address.
 * Arguments:
 * - cursorPosition: Where to print interface message.
 * - inaddr: IP address to stringify in order to print it in console
 *   interface.
 * - ipAddrTable: Same as above.
 */
static void
write_info_one_available_addr(COORD cursorPosition[2], struct in_addr* inaddr, MIB_IPADDRTABLE ipAddrTable[4]) {
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

/*
 * Function description:
 * - If two IP address are available for the server socket wu print these two
 *   address in user interface message. wu prompt the user to select one
 *   address. After wu prompt the user to connect to the address he has
 *   chosen.
 * Arguments:
 * - cursorPosition: Where to print interface message.
 * - inaddr: IP address to stringify in order to print it in console
 *   interface.
 * - ipAddrTable: Same as above.
 */
static void
ui_two_available_addr(COORD cursorPosition[2], struct in_addr* inaddr, MIB_IPADDRTABLE ipAddrTable[4]) {
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


	inaddr1.s_addr = (addrRow + 1)->dwAddr;
	inaddr2.s_addr = (addrRow + 2)->dwAddr;

	ZeroMemory(strIpAddrAvail1, 255);
	ZeroMemory(strIpAddrAvail2, 255);

	cursorPosition[0].Y += 2;

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_MSG_TWO_AVAILABLE_ADDR, NULL);

	// cursorPosition[0].X += 4;
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

		/* Replace atoi on a single keypress with a direct, safe conversion.
		 * The console input provides a single ASCII character; convert it to
		 * a numeric value only if it is a digit. This avoids buffer issues
		 * and unnecessary C runtime parsing overhead.
		 */
		{
			CHAR ch = inRec.Event.KeyEvent.uChar.AsciiChar;
			if (ch >= '0' && ch <= '9') {
				dwUsrChoice = (DWORD)(ch - '0');
			} else {
				dwUsrChoice = 0; /* treat non-digit as invalid choice */
			}
		}
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
		inaddr->s_addr = (addrRow + 2)->dwAddr;

		cursorPosition[0].X += 5;
		cursorPosition[0].Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
		write_info_in_console(INF_WIFIUPLOAD_HTTP_LISTEN, inet_ntoa(inaddr2));
		cursorPosition[0].X -= 5;
	}

	return;
}


/*
 * Function description:
 * - Get IP address that are available then there is two case wu support. There
 *   is two address available or there is three address available. Handle these
 *   two case with dedicated function. wu doesn't support more than three IP
 *   address available.
 * Arguments:
 * - cursorPosition: Position where display error message.
 * - inaddr: Used to hold raw IP address.
 * Return value:
 * - dwNumEntries: Number of IP address available.
 */

DWORD available_address_ui(COORD cursorPosition[2], struct in_addr* inaddr) {
	MIB_IPADDRTABLE ipAddrTable[4];
	ULONG sizeIpAddrTable = sizeof(MIB_IPADDRTABLE) * 4;
	int ret;

	ZeroMemory(&ipAddrTable, sizeIpAddrTable);
	ret = GetIpAddrTable((PMIB_IPADDRTABLE)&ipAddrTable, &sizeIpAddrTable, TRUE);

	if (ret != NO_ERROR || ipAddrTable[0].dwNumEntries < 2)
		show_error_wait_close(&cursorPosition[0], ERR_MSG_CONNECTIVITY, NULL, 0);
	else if (ipAddrTable[0].dwNumEntries == 2)
		write_info_one_available_addr(cursorPosition, inaddr, ipAddrTable);
	else if (ipAddrTable[0].dwNumEntries == 3)
		ui_two_available_addr(cursorPosition, inaddr, ipAddrTable);
	else
		show_error_wait_close(&cursorPosition[0], ERR_MSG_TOO_MANY_ADDR, NULL, 0);


	return ipAddrTable->dwNumEntries;
}

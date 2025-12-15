#include <WinSock2.h>
#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"
#include "wu_main.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_txstats.h"
#include "wu_http.h"
#include "wu_http_loop.h"
#include "wu_content.h"


extern const struct _http_resources http_resources[];
extern struct wu_msg wumsg[];
extern HANDLE g_hConsoleOutput;
extern HANDLE g_hNewFile_tmp;
extern char g_sNewFile_tmp[1024];


static HANDLE
create_userfile_tmp(COORD* cursorPosition,
	char* filename,
	char* userfile_tmp);
static errno_t
receive_MIME_header(struct user_stats* upstats,
	int s, unsigned short* MIMELen);



static HANDLE
create_userfile_tmp(COORD* cursorPosition,
	char* filename,
	char* userfile_tmp)
{
	char download_dir[1024];
	HANDLE hFile;

	ZeroMemory(userfile_tmp, FILENAME_MAX_SIZE + 6 + 1024);

	build_download_directory(download_dir);

	strcpy_s(userfile_tmp, 1024, &download_dir[0]);
	strcat_s(userfile_tmp, 1024, filename);
	strcat_s(userfile_tmp, 1024, ".tmp");


	hFile = CreateFile(userfile_tmp, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		INPUT_RECORD inRec;
		DWORD err;
		DWORD read;

		cursorPosition->Y += 3;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);

		err = GetLastError();
		write_info_in_console(ERR_MSG_CANNOT_CREATE_FILE, (void*)&err, 0);

		cursorPosition->Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

	}

	g_hNewFile_tmp = hFile;

	ZeroMemory(g_sNewFile_tmp, 1024);
	strcpy_s(g_sNewFile_tmp, 1024, userfile_tmp);

	return hFile;
}

static errno_t
receive_MIME_header(struct user_stats* upstats, int s, unsigned short* MIMElen)
{
	int ret;
	unsigned short i;
	CHAR buffer[2048];
	char* p_MIME_filename;
	char* p_quote;

	ZeroMemory(buffer, 2048);
	for (i = 0; i < 2043; i++) {
		ret = recv(s, &buffer[i], 1, 0);
		if (ret != 1)
			return -1;
		if (buffer[i] == '\r') {
			ret = recv(s, &buffer[++i], 1, 0);
			if (ret != 1)
				return -1;
			if (buffer[i] == '\n') {
				ret = recv(s, &buffer[++i], 1, 0);
				if (ret != 1)
					return -1;
				if (buffer[i] == '\r') {
					ret = recv(s, &buffer[++i], 1, 0);
					if (ret != 1)
						return -1;
					if (buffer[i] == '\n')
						break;
				}
			}
		}
	}

	if (i >= 2043)
		return EINVAL;

	*MIMElen = i;

	p_MIME_filename = strstr(buffer, "filename=\"");
	if (p_MIME_filename == NULL)
		return EINVAL;

	p_MIME_filename += (sizeof("filename=\"") - 1);

	p_quote = strchr(p_MIME_filename, '"');
	if (p_quote == NULL)
		return EINVAL;

	*p_quote = '\0';

	if (strcpy_s(upstats->filename, FILENAME_MAX_SIZE, p_MIME_filename) != 0)
		return EINVAL;

	if (strchr(upstats->filename, '\\') != NULL)
		return EINVAL;

	return 0;
}

int
receive_file(COORD* cursorPosition,
	struct header_nv* httpnv, int s,
	struct user_stats* upstats, int theme,
	int* bytesent) {
	unsigned short MIMElen, boundarylen;
	HANDLE hFile;
	DWORD written, tick_start, tick_end, tick_diff;
	char* pboundary;
	char boundary[64];
	u_int64 content_length;
	struct tx_stats txstats;
	char buffer[1024];
	COORD coordPerCent;
	COORD coordAverageTX;
	struct http_resource httpres;
	struct success_info successinfo;
	char userfile_tmp[FILENAME_MAX_SIZE + 6 + 1024];
	char* newFile;
	LARGE_INTEGER len_li;
	u_int64 sizeNewFile = 0;
	u_int64 sizeNewFileDup = 0;
	float average_speed;
#ifdef VERSION_FR
	const CCHAR* units[] = { "Octets", "KO", "MO", "GO", "TO", "" };
#else
	const CCHAR* units[] = { "Bytes", "KB", "MB", "GB", "TB", "" };
#endif
	unsigned char idxunit;
	int ret;
	int ires;

	ZeroMemory(&httpres, sizeof(struct http_resource));
	ret = nv_find_name_client(httpnv, "Content-Type");
	if (ret < 0)
		return -1;

	pboundary = strstr((httpnv + ret)->value.v, "boundary=");
	if (pboundary == NULL)
		return -1;

	pboundary += (sizeof("boundary=") - 1);
	boundarylen = (unsigned short)strlen(pboundary);
	if (*pboundary == '\0' || boundarylen < 7 || boundarylen > 63)
		return -1;

	ZeroMemory(boundary, 64);
	if (strcpy_s(boundary, 64, pboundary) != 0)
		return -1;

	ret = nv_find_name_client(httpnv, "Content-Length");
	if (ret < 0)
		return -1;

	content_length = _atoi64((httpnv + ret)->value.v);

	if (receive_MIME_header(upstats, s, &MIMElen) != 0)
		return -1;

	if (strlen(upstats->filename) == 0)
		return -1;

	clear_txrx_pane(cursorPosition);

	coordAverageTX.X = cursorPosition->X;
	coordAverageTX.Y = cursorPosition->Y + 1;
	SetConsoleTextAttribute(g_hConsoleOutput, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	SetConsoleTextAttribute(g_hConsoleOutput, 0);
	cursorPosition->X++;

	hFile = create_userfile_tmp(cursorPosition, upstats->filename, userfile_tmp);

	ZeroMemory(&txstats, sizeof(struct tx_stats));
	GetSystemTime(&txstats.start);
	tick_start = GetTickCount();

	cursorPosition->Y += 2;
	cursorPosition->X--;
	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
	write_info_in_console(INF_WIFIUPLOAD_UI_FILE_DOWNLOAD, NULL, 0);
	WriteConsoleA(g_hConsoleOutput, upstats->filename, (DWORD)strlen(upstats->filename), &written, NULL);
	cursorPosition->Y -= 2;
	cursorPosition->X++;

	content_length -= (MIMElen + 1);
	txstats.total_size = content_length;
	txstats.one_percent = (long long)txstats.total_size / 100;
	txstats.curr_percent = txstats.curr_percent_bak = 0;
	txstats.received_size = txstats.received_size_bak = 0;

	coordPerCent.X = cursorPosition->X + 52;
	coordPerCent.Y = cursorPosition->Y;

	SetConsoleCursorPosition(g_hConsoleOutput, coordPerCent);
	write_info_in_console(INF_ZERO_PERCENT, NULL, 0);
	while (content_length > 0) {
		if (content_length < (1024 + boundarylen + 8) && content_length > 1024) {
			ret = recv(s, buffer, 777, MSG_WAITALL);
			if (ret <= 0)
				break;
			WriteFile(hFile, buffer, ret, &written, NULL);
			content_length -= ret;
		}
		else if (content_length <= 1024) {
			ret = recv(s, buffer, (USHORT)content_length, MSG_WAITALL);
			if (ret <= 0)
				break;
			WriteFile(hFile, buffer, ret - boundarylen - 8, &written, NULL);
			content_length -= ret;
			break;
		}
		else {
			ret = recv(s, buffer, 1024, MSG_WAITALL);
			if (ret <= 0)
				break;
			WriteFile(hFile, buffer, ret, &written, NULL);
			content_length -= ret;
		}

		txstats.received_size += (unsigned int)ret;

		GetSystemTime(&txstats.current);

		if (txstats.current.wHour > txstats.currentbak.wHour ||
			txstats.current.wMinute > txstats.currentbak.wMinute ||
			txstats.current.wSecond > txstats.currentbak.wSecond) {
			double averageRateTX;
			CHAR strAverageRateTX[42];


			memcpy(&txstats.currentbak, &txstats.current, sizeof(SYSTEMTIME));

			averageRateTX = (txstats.received_size - txstats.received_size_bak) / 1000.000;

			SetConsoleCursorPosition(g_hConsoleOutput, coordAverageTX);

			ZeroMemory(strAverageRateTX, 42);

			if (averageRateTX > 1000) {
				averageRateTX /= 1000.000;
				if (averageRateTX > 1000) {
					averageRateTX /= 1000.000;
					sprintf_s(strAverageRateTX, 42, "%0.2f", averageRateTX);
					write_info_in_console(INF_WIFIUPLOAD_TX_SPEED_UI_GO, strAverageRateTX, 0);
				}
				else {
					sprintf_s(strAverageRateTX, 42, "%0.2f", averageRateTX);
					write_info_in_console(INF_WIFIUPLOAD_TX_SPEED_UI_MO, strAverageRateTX, 0);
				}
			}
			else {
				sprintf_s(strAverageRateTX, 42, "%0.2f", averageRateTX);
				write_info_in_console(INF_WIFIUPLOAD_TX_SPEED_UI_KO, strAverageRateTX, 0);
			}

			txstats.received_size_bak = txstats.received_size;

			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		}

		txstats.curr_percent = (u_char)(((float)txstats.received_size / (float)txstats.total_size) * 100);
		if (txstats.curr_percent > txstats.curr_percent_bak + 2) {
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(INF_WIFIUPLOAD_ONE_PBAR, NULL, 0);
			cursorPosition->X++;
			SetConsoleCursorPosition(g_hConsoleOutput, coordPerCent);
			write_info_in_console(INF_WIFIUPLOAD_CURRENT_PERCENT, (void*)txstats.curr_percent, 0);
			txstats.curr_percent_bak += 2;
		}

	}


	if (content_length != 0) {
		CloseHandle(hFile);
		DeleteFileA(userfile_tmp);
		cursorPosition->Y += 3;
		cursorPosition->X = (cursorPosition + 1)->X;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_FAIL_TX, NULL, 0);
		cursorPosition->Y++;
		return -1;
	}

	SetConsoleCursorPosition(g_hConsoleOutput, coordPerCent);
	write_info_in_console(INF_CENT_PERCENT, NULL, 0);

	GetSystemTime(&txstats.end);

	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
	SetConsoleTextAttribute(g_hConsoleOutput, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_INTENSITY);
	GetFileSizeEx(hFile, &len_li);

	if (len_li.HighPart != 0)
		sizeNewFile = ((u_int64)len_li.HighPart << 32) | len_li.LowPart;
	else
		sizeNewFile = (u_int64)len_li.LowPart;

	sizeNewFileDup = sizeNewFile;

	CloseHandle(hFile);

	cursorPosition->Y += 2;
	cursorPosition->X = (cursorPosition + 1)->X;

	newFile = _strdup(userfile_tmp);
	*(strrchr(newFile, '.')) = '\0';
	MoveFileExA(userfile_tmp, newFile, MOVEFILE_REPLACE_EXISTING);
	LocalFree(newFile);

	ZeroMemory(&successinfo, sizeof(struct success_info));
	strcpy_s(successinfo.filename, FILENAME_MAX_SIZE, upstats->filename);

	for (idxunit = 0; sizeNewFile > 1024; sizeNewFile /= 999, ++idxunit);

	StringCchPrintfA(successinfo.filenameSize, 24, "%u", sizeNewFile);
	
	strcat_s(successinfo.filenameSize, 24 - strlen(successinfo.filenameSize), " ");
	strcat_s(successinfo.filenameSize, 24 - strlen(successinfo.filenameSize), units[idxunit]);

	sizeNewFile = sizeNewFileDup;
	average_speed = 0.0;

	tick_end = GetTickCount();
	tick_diff = tick_end - tick_start;

	if (tick_diff < 1000)
		sprintf_s(successinfo.elapsedTime, 24, "%u msecs", tick_diff);
	else if (tick_diff < 1000 * 60)
		sprintf_s(successinfo.elapsedTime, 24, "%.2f secs",
			((float)tick_diff / (1000.0)));
	else if (tick_diff < 1000 * 60 * 60)
		sprintf_s(successinfo.elapsedTime, 24, "%.2f min",
			((float)tick_diff / (1000.0 * 60)));
	else if (tick_diff < 1000 * 60 * 60 * 60)
		sprintf_s(successinfo.elapsedTime, 24, "%.2f h",
			((float)tick_diff / (1000.0 * 60 * 60)));


	average_speed = ((float)sizeNewFile / ((tick_diff > 1000 ? tick_diff : 1000) / 1000));

	if (average_speed > 99999999.0)
		sprintf_s(successinfo.averagespeed, 24, EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_GO, average_speed / 1000000000.00);
	else if (average_speed > 999999.0)
		sprintf_s(successinfo.averagespeed, 24, EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_MO, average_speed / 1000000.00);
	else
		sprintf_s(successinfo.averagespeed, 24, EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_KO, average_speed / 100.00);

	ZeroMemory(&httpres, sizeof(struct http_resource));
	for (ires = 0; http_resources[ires].resource != NULL; ires++) {
		if (strcmp(http_resources[ires].resource, "success") == 0)
			break;
	}

	create_local_resource(&httpres, ires, theme);
	http_serv_resource(&httpres, s, &successinfo, bytesent, 200);

	return 0;
}


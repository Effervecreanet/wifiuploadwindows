#include <WinSock2.h>
#include <Windows.h>
#include <strsafe.h>
#include <stdlib.h>
#include <stdint.h>

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>


#include "wu_msg.h"
#include "wu_main.h"
#include "wu_http_nv.h"
#include "wu_txstats.h"
#include "wu_http_loop.h"
#include "wu_content.h"
#include "wu_http.h"
#include "wu_http_receive.h"
#include "wu_tls_https_receive.h"


extern const struct _http_resources http_resources[];
extern struct wu_msg wumsg[];
extern HANDLE g_hConsoleOutput;
extern HANDLE g_hNewFile_tmp;
extern char g_sNewFile_tmp[1024];
extern FILE* g_fphttpslog;


static HANDLE create_userfile_tmp(COORD* cursorPosition, char* filename, char* userfile_tmp);
static errno_t receive_MIME_header(struct user_stats* upstats, SOCKET s, unsigned short* MIMELen);
static int get_MIME_boundarylen(struct header_nv* httpnv, unsigned short* boundarylen);
static errno_t get_MIME_filename(struct user_stats* upstats, char** req_buffer, unsigned int req_buffer_size, unsigned short* MIMElen);
static void wcons_pbar_first_char(COORD* cursorPosition);
static void wcons_pbar_last_char(COORD* cursorPosition);
static void wcons_ui_file_line(COORD* cursorPosition, char* filename);
static void init_tx_stats(struct tx_stats* txstats, u_int64 content_length);
static void wcons_zero_percent(COORD* cursorPosition, COORD *coordPerCent);
static void wcons_cent_percent(COORD coordPerCent);
static int tls_recv_file(HANDLE hFile, CtxtHandle *ctxtHandle, SOCKET s, u_int64 *received_size, u_int64 *content_length, unsigned short boundarylen, COORD *cursorPosition);



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
	if (hFile == INVALID_HANDLE_VALUE)
		show_error_wait_close(cursorPosition, ERR_MSG_CANNOT_CREATE_FILE, NULL, GetLastError());

	g_hNewFile_tmp = hFile;

	ZeroMemory(g_sNewFile_tmp, 1024);
	strcpy_s(g_sNewFile_tmp, 1024, userfile_tmp);

	return hFile;
}

static int
get_MIME_boundarylen(struct header_nv* httpnv, unsigned short* boundarylen)
{
	int ret;
	char* pboundary;

	ret = nv_find_name_client(httpnv, "Content-Type");
	if (ret < 0)
		return -1;

	pboundary = strstr((httpnv + ret)->value.v, "boundary=");
	if (pboundary == NULL)
		return -1;

	pboundary += (sizeof("boundary=") - 1);
	*boundarylen = (unsigned short)strlen(pboundary);

	if (*pboundary == '\0' || *boundarylen < 7 || *boundarylen > 63)
		return -1;

	return 0;
}

static errno_t
get_MIME_filename(struct user_stats* upstats, char** req_buffer, unsigned int req_buffer_size, unsigned short* MIMElen)
{
	int ret;
	unsigned short i;
	char* p_MIME_filename;
	char* p_quote;
	char* p_MIMEend;
	char* buffer;

	buffer = malloc(req_buffer_size + 1);
	ZeroMemory(buffer, req_buffer_size + 1);
	memcpy(buffer, *req_buffer, req_buffer_size);

	p_MIMEend = strstr(buffer, "\r\n\r\n");
	if (p_MIMEend == NULL) {
		free(buffer);
		return -1;
	}

	*(p_MIMEend + 3) = '\0';
	*MIMElen = strlen(buffer);
	*(p_MIMEend + 3) = '\n';

	p_MIME_filename = strstr(buffer, "filename=\"");
	if (p_MIME_filename == NULL) {
		free(buffer);
		return EINVAL;
	}

	p_MIME_filename += (sizeof("filename=\"") - 1);

	p_quote = strchr(p_MIME_filename, '"');
	if (p_quote == NULL) {
		free(buffer);
		return EINVAL;
	}

	*p_quote = '\0';

	if (strcpy_s(upstats->filename, FILENAME_MAX_SIZE, p_MIME_filename) != 0) {
		free(buffer);
		return EINVAL;
	}

	if (strlen(upstats->filename) == 0)
		return -1;

	if (strchr(upstats->filename, '\\') != NULL) {
		free(buffer);
		return EINVAL;
	}

	*MIMElen += 1;

	free(buffer);

	return 0;
}

static void
wcons_pbar_first_char(COORD* cursorPosition) {
	DWORD written;

	SetConsoleTextAttribute(g_hConsoleOutput, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	SetConsoleTextAttribute(g_hConsoleOutput, 0);
	cursorPosition->X++;

	return;
}

static void
wcons_pbar_last_char(COORD* cursorPosition) {
	DWORD written;

	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
	SetConsoleTextAttribute(g_hConsoleOutput, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE);
	WriteConsoleA(g_hConsoleOutput, " ", 1, &written, NULL);
	SetConsoleTextAttribute(g_hConsoleOutput, FOREGROUND_INTENSITY);

	return;
}

static void
wcons_ui_file_line(COORD* cursorPosition, char* filename) {
	DWORD written;

	cursorPosition->Y += 2;
	cursorPosition->X--;

	SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
	write_info_in_console(INF_WIFIUPLOAD_UI_FILE_DOWNLOAD, NULL, 0);
	WriteConsoleA(g_hConsoleOutput, filename, (DWORD)strlen(filename), &written, NULL);

	cursorPosition->Y -= 2;
	cursorPosition->X++;

	return;
}

static void
init_tx_stats(struct tx_stats* txstats, u_int64 content_length) {
	ZeroMemory(txstats, sizeof(struct tx_stats));
	GetSystemTime(&txstats->start);

	txstats->total_size = content_length;
	txstats->one_percent = (long long)txstats->total_size / 100;
	txstats->curr_percent = txstats->curr_percent_bak = 0;
	txstats->received_size = txstats->received_size_bak = 0;

	return;
}

static void
wcons_zero_percent(COORD* cursorPosition, COORD *coordPerCent) {
	coordPerCent->X = cursorPosition->X + 52;
	coordPerCent->Y = cursorPosition->Y;

	SetConsoleCursorPosition(g_hConsoleOutput, *coordPerCent);
	write_info_in_console(INF_ZERO_PERCENT, NULL, 0);

	return;
}

static void
wcons_cent_percent(COORD coordPerCent) {
	SetConsoleCursorPosition(g_hConsoleOutput, coordPerCent);
	write_info_in_console(INF_CENT_PERCENT, NULL, 0);
	return;
}

static int
tls_recv_file(HANDLE hFile, CtxtHandle *ctxtHandle, SOCKET s, u_int64 *received_size, u_int64 *content_length, unsigned short boundarylen, COORD *cursorPosition) {
	char* tls_recv_output;
	unsigned int tls_recv_output_size;
	DWORD written;

	if (tls_recv(ctxtHandle, s, &tls_recv_output, &tls_recv_output_size, cursorPosition) < 0)
		return -1;

	*content_length -= tls_recv_output_size;

	if (*content_length == 0) {
		WriteFile(hFile, tls_recv_output, tls_recv_output_size - boundarylen - 8, &written, NULL);
		free(tls_recv_output);
		return 0;
	}
	else
		WriteFile(hFile, tls_recv_output, tls_recv_output_size, &written, NULL);

	*received_size += tls_recv_output_size;

	free(tls_recv_output);

	return 1;
}



int
tls_receive_file(COORD* cursorPosition, struct header_nv* httpnv, SOCKET s, struct user_stats* upstats, int theme, int* bytesent, CtxtHandle* ctxtHandle) {
	unsigned short MIMElen, boundarylen;
	HANDLE hFile;
	DWORD written, tick_start, tick_end, tick_diff;
	char* pboundary;
	char boundary[64];
	u_int64 content_length;
	struct tx_stats txstats;
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
	char* tls_recv_output;
	unsigned int tls_recv_output_size;

	ret = get_MIME_boundarylen(httpnv, &boundarylen);
	if (ret < 0)
		return -1;

	ret = nv_find_name_client(httpnv, "Content-Length");
	if (ret < 0)
		return -1;

	content_length = _atoi64((httpnv + ret)->value.v);
	if (content_length <= 0)
		return -1;

	if (tls_recv(ctxtHandle, s, &tls_recv_output, &tls_recv_output_size, cursorPosition) < 0)
		return -1;
	
	if (get_MIME_filename(upstats, &tls_recv_output, tls_recv_output_size, &MIMElen) != 0) {
		free(tls_recv_output);
		return -1;
	}

	clear_txrx_pane(cursorPosition);

	coordAverageTX.X = cursorPosition->X;
	coordAverageTX.Y = cursorPosition->Y + 1;

	wcons_pbar_first_char(cursorPosition);

	hFile = create_userfile_tmp(cursorPosition, upstats->filename, userfile_tmp);

	wcons_ui_file_line(cursorPosition, upstats->filename);

	tick_start = GetTickCount();
	content_length -= tls_recv_output_size;

	init_tx_stats(&txstats, content_length);

	wcons_zero_percent(cursorPosition, &coordPerCent);

	if (content_length == 0)
		WriteFile(hFile, tls_recv_output + MIMElen, tls_recv_output_size - MIMElen - boundarylen - 8, &written, NULL);
	else
		WriteFile(hFile, tls_recv_output + MIMElen, tls_recv_output_size - MIMElen, &written, NULL);
	
	free(tls_recv_output);

	while (tls_recv_file(hFile, ctxtHandle, s, &txstats.received_size, &content_length, boundarylen, cursorPosition))
		wcons_upload_info(&txstats, coordAverageTX, cursorPosition, coordPerCent);

	if (content_length != 0) {
		CloseHandle(hFile);
		DeleteFileA(userfile_tmp);
		cursorPosition->Y += 3;
		cursorPosition->X = (cursorPosition + 1)->X;
		show_error_wait_close(cursorPosition, ERR_MSG_FAIL_TX, NULL, 0);
	}

	wcons_cent_percent(coordPerCent);

	GetSystemTime(&txstats.end);

	wcons_pbar_last_char(cursorPosition);

	GetFileSizeEx(hFile, &len_li);

	if (len_li.HighPart != 0)
		sizeNewFile = ((u_int64)len_li.HighPart << 32) | len_li.LowPart;
	else
		sizeNewFile = (u_int64)len_li.LowPart;

	sizeNewFileDup = sizeNewFile;

	CloseHandle(hFile);
	g_hNewFile_tmp = INVALID_HANDLE_VALUE;

	cursorPosition->Y += 2;
	cursorPosition->X = (cursorPosition + 1)->X;

	newFile = _strdup(userfile_tmp);
	*(strrchr(newFile, '.')) = '\0';
	MoveFileExA(userfile_tmp, newFile, MOVEFILE_REPLACE_EXISTING);
	free(newFile);

	ZeroMemory(&successinfo, sizeof(struct success_info));
	strcpy_s(successinfo.filename, FILENAME_MAX_SIZE, upstats->filename);

	for (idxunit = 0; sizeNewFile > 1024; sizeNewFile /= 999, ++idxunit);

	StringCchPrintfA(successinfo.filenameSize, 24, "%llu", sizeNewFile);

	strcat_s(successinfo.filenameSize, 24, " ");
	strcat_s(successinfo.filenameSize, 24, units[idxunit]);

	sizeNewFile = sizeNewFileDup;
	average_speed = 0.0;

	chrono(&successinfo, tick_start, sizeNewFile);

	ZeroMemory(&httpres, sizeof(struct http_resource));
	for (ires = 0; http_resources[ires].resource != NULL; ires++) {
		if (strcmp(http_resources[ires].resource, "success") == 0)
			break;
	}

	create_local_resource(&httpres, ires, theme);
	https_serv_resource(&httpres, s, &successinfo, bytesent, ctxtHandle, *cursorPosition);

	return 0;
}

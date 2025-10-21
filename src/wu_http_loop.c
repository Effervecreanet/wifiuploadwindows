#include <Windows.h>
#include <strsafe.h>
#include <time.h>

#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_loop.h"
#include "wu_content.h"
#include "wu_http_theme.h"


extern const struct _http_resources http_resources[];
extern struct wu_msg wumsg[];
extern FILE* g_fplog;
extern int* g_usersocket;
extern HANDLE g_hConsoleOutput;
extern HANDLE g_hNewFile_tmp;
extern int *g_listensocket;


int
http_match_resource(char* res)
{
	int i;

	if (res && *res == '/' && *(res + 1) == '\0') {
		return 0;
	}
	else if (res && *res && strcmp(res + 1, "favicon.ico") == 0) {
		return 1;
	}
	for (i = 2; http_resources[i].resource != NULL; i++) {
		if (strcmp(res + 1, http_resources[i].resource) == 0)
			return i;
	}

	return -1;
}

int
create_local_resource(struct http_resource* lres, int ires, int theme) {
	char curDir[1024];

	ZeroMemory(curDir, 1024);
	if (!GetCurrentDirectoryA(1024, curDir)) {
		return 1;
	}

	if (strcmp(http_resources[ires].type, "text/html") == 0) {
#ifdef VERSION_FR
		strcat_s(curDir, 1024, "\\html\\fr\\");
#else
		strcat_s(curDir, 1024, "\\html\\en\\");
#endif
		if (theme == 0)
			strcat_s(curDir, 1024, "light\\");
		else
			strcat_s(curDir, 1024, "dark\\");

		strcat_s(curDir, 1024, http_resources[ires].resource);
	}
	else if (strcmp(http_resources[ires].type, "image/png") == 0) {
		strcat_s(curDir, 1024, "\\");
		if (theme == 0)
			strcat_s(curDir, 1024, http_resources[ires].path_theme1);
		else
			strcat_s(curDir, 1024, http_resources[ires].path_theme2);
	}
	else if (strcmp(http_resources[ires].type, "image/x-icon") == 0) {
		strcat_s(curDir, 1024, "\\");
		strcat_s(curDir, 1024, http_resources[ires].path_theme1);
	}


	strcpy_s(lres->type, HTTP_TYPE_MAX_LENGTH, http_resources[ires].type);
	strcpy_s(lres->resource, HTTP_RESSOURCE_MAX_LENGTH - 1, curDir);

	return 0;
}

static void
check_cookie_theme(struct header_nv hdrnv[], int* theme) {
	int res;

	res = nv_find_name_client(hdrnv, "Cookie");
	if (res < 0) {
		*theme = 0;
		return;
	}
	else if (strncmp(hdrnv[res].value.v, "theme=dark", sizeof("theme=dark") - 1) == 0) {
		*theme = 1;
	}
	else if (strncmp(hdrnv[res].value.v, "theme=light", sizeof("theme=light") - 1) == 0) {
		*theme = 0;
	}

	return;
}

static void
wu_404_response(COORD cursorPosition[2], struct header_nv *httpnv, int *theme, int s_user, int *bytesent) {
	int ires;
	struct http_resource httplocalres;

	check_cookie_theme(httpnv, theme);

	for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

	ZeroMemory(&httplocalres, sizeof(struct http_resource));
	if (create_local_resource(&httplocalres, ires, *theme) != 0) {
		INPUT_RECORD inRec;
		DWORD read;

		cursorPosition->Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	http_serv_resource(&httplocalres, s_user, NULL, bytesent, 404);

	return;
}

static void
wu_quit_response(COORD cursorPosition[2], struct header_nv *httpnv, int *theme, int s_user, int *bytesent) {
	int ires;
	struct http_resource httplocalres;

	check_cookie_theme(httpnv, theme);

	for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

	ZeroMemory(&httplocalres, sizeof(struct http_resource));
	if (create_local_resource(&httplocalres, ires, *theme) != 0) {
		INPUT_RECORD inRec;
		DWORD read;

		cursorPosition->Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	http_serv_resource(&httplocalres, s_user, NULL, bytesent, 404);

	return;
}
static void
quit_wu(int s_user) {
	closesocket(s_user);
	closesocket(*g_listensocket);
	CloseHandle(g_hConsoleOutput);
	fclose(g_fplog);
	WSACleanup();
	ExitProcess(0);

	return;
}

static void
show_download_directory(void) {
	char dd[1024];

	build_download_directory(dd);
	ShellExecuteA(NULL, "open", dd, NULL, NULL, SW_SHOWNORMAL);

	return;
}

static int
handle_theme_change(struct header_nv *httpnv, int s_user, int *theme) {
	char cookie[48];

	if (wu_http_recv_theme(httpnv, s_user, theme) < 0)
		return -1;

	ZeroMemory(cookie, 48);
	if (*theme == 0)
		strcpy_s(cookie, 48, "theme=dark");
	else
		strcpy_s(cookie, 48, "theme=light");

	if (apply_theme(s_user, cookie) < 0)
		return -1;

	return 0;
}

static void
create_send_resource(struct header_nv *httpnv, int s_user, int *bytesent, int theme, int resource_index, COORD cursorPosition[2]) {
		struct http_resource httplocalres;
		int err;

		check_cookie_theme(httpnv, &theme);

		ZeroMemory(&httplocalres, sizeof(struct http_resource));
		if (create_local_resource(&httplocalres, resource_index, theme) != 0) {
			INPUT_RECORD inRec;
			DWORD read;

			cursorPosition->Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}

		err = http_serv_resource(&httplocalres, s_user, NULL, bytesent, 200);
		if (err > 1) {
			INPUT_RECORD inRec;
			DWORD read;

			cursorPosition->Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_FMT_MSG_CANNOT_SERV_RESOURCE, (void*)&err);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}
		else if (err == 0) {
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(INF_MSG_INCOMING_CONNECTION, NULL);
			cursorPosition->Y++;
		}

	return;
}

static int
handle_post_request(struct http_reqline *reqline, struct header_nv *httpnv, int s_user,
						int *bytesent, int theme, COORD cursorPosition[2]) {
	int err;

		if (strcmp(reqline->resource + 1, "theme") == 0) {
			err = handle_theme_change(httpnv, s_user, &theme);
			if (err < 0)
				return -1;
		} else if (strcmp(reqline->resource + 1, "upload") == 0) {
			struct user_stats upstats;

			clear_txrx_pane(cursorPosition);
			check_cookie_theme(httpnv, &theme);

			ZeroMemory(&upstats, sizeof(struct user_stats));
			err = receive_file(cursorPosition, httpnv, s_user, &upstats, theme, bytesent);

			cursorPosition->Y++;
			if (err < 0)
				return -1;
				
		}

	return 0;
}

static void
create_log_entry(char *logentry, char *ipaddrstr, struct http_reqline *reqline, int bytesent, unsigned int status_code) {
	struct tm tmval;
	time_t wutime;
	char log_timestr[42];

	ZeroMemory(logentry, 256);
	ZeroMemory(log_timestr, 42);

	time(&wutime);

	ZeroMemory(&tmval, sizeof(struct tm));
	localtime_s(&tmval, &wutime);

	strftime(log_timestr, 42, "%d/%b/%Y:%T -0600", &tmval);
	sprintf_s(logentry, 256, "%s - - [%s] \"%s %s %s\" %i %i\n", ipaddrstr, log_timestr,
		reqline->method, reqline->resource,
		reqline->version, status_code, bytesent);

	return;
}



int
http_loop(COORD* cursorPosition, struct in_addr* inaddr, int s, char logentry[256]) {
	struct http_reqline reqline;
	struct header_nv httpnv[HEADER_NV_MAX_SIZE];
	int s_user;
	DWORD err;
	int theme = 0;
	char ipaddrstr[16];
	int bytesent;
	int i, resource_index;

	bytesent = 0;
	memset(ipaddrstr, 0, 16);
	s_user = accept_conn(cursorPosition, s, ipaddrstr);

	g_usersocket = &s_user;

	ZeroMemory(&reqline, sizeof(struct http_reqline));
	if (http_recv_reqline(&reqline, s_user) != 0)
		goto err;

	ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
	if (http_recv_headernv(httpnv, s_user) != 0)
		goto err;

	i = nv_find_name_client(httpnv, "Host");
	if (i < 0 || strcmp(httpnv[i].value.v, inet_ntoa(*inaddr)) != 0)
		goto err;

	if (strcmp(reqline.method, "GET") == 0) {
		resource_index = http_match_resource(reqline.resource);

		if (resource_index < 0) {
			wu_404_response(cursorPosition, httpnv, &theme, s_user, &bytesent);
			create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 404);
			goto err;
		} else if (strcmp(reqline.resource + 1, "quit") == 0) {
			create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 200);
			wu_quit_response(cursorPosition, httpnv, &theme, s_user, &bytesent);
			quit_wu(s_user);
		} else if (strcmp(reqline.resource + 1, "openRep") == 0) {
			show_download_directory();

			if (strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/index") != 0)
				goto err;

			resource_index = 0;
		}

		create_send_resource(httpnv, s_user, &bytesent, theme, resource_index, cursorPosition);
		create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 200);
	} else if (strcmp(reqline.method, "POST") == 0) {
		if (handle_post_request(&reqline, httpnv, s_user, &bytesent,
							theme, cursorPosition) < 0)
			goto err;

		create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 200);
	}


err:
	closesocket(s_user);
	s_user = 0;
	g_hNewFile_tmp = INVALID_HANDLE_VALUE;

	return 0;
}


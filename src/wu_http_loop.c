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
extern int* g_listensocket;


/*
 * Function description:
 * - Compare user requested resource against local resource array.
 * Arguments:
 * - res: Requested resource.
 * Return value:
 * - -1: No resource can be found.
 * - i: Local resource index in array
 */
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

/*
 * Function description:
 * - Map requested resource to local resource. There is three case: 1) Resource is text/html one
 *   2) Resource is image/png 3) Resource is icon. Local resource is in Program Files application
 *   directory.
 * Arguments:
 * - lres: Final local resource to serv
 * - ires: Index for http local resource array
 * - theme: Theme value used to build local resource path
 * Return Value:
 * - 0: No error occured.
 * - 1: An error occured.
 */
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

/*
 * Function description:
 * - Parse the cookie header field to retrieve theme information value.
 * Arguments:
 * - hdr_nv: Header names values used to search "Cookie" value.
 * - theme: Address pointer that store theme value ("light" or "dark")
 */
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

/*
 * Function description:
 * - Send "error 404" html page with 404 http status code.
 * Arguments:
 * - cursorPosition: Position of the cursor to print information related to an error. If one occurs.
 * - httpnv: Used to check cookie header field. Cookie header field contains theme information.
 * - theme: Is it dark or light theme in use.
 * - s_user: Client socket to send data to.
 * - bytesent: Total byte sent to user.
 */
static void
wu_404_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, int s_user, int* bytesent) {
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

/*
 * Function description:
 * - Send "erreur 404" html page with 200 http status code.
 * Arguments:
 * - cursorPosition: Position of the cursor to print information related to an error. If one occurs.
 * - httpnv: Used to check cookie header field. Cookie header field contains theme information.
 * - theme: Is it dark or light theme in use.
 * - s_user: Client socket to send data to.
 * - bytesent: Total byte sent to user.
 */
static void
wu_quit_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, int s_user, int* bytesent) {
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

	http_serv_resource(&httplocalres, s_user, NULL, bytesent, 200);

	return;
}

/*
 * Function description:
 * - Close sockets, close file and close winsock. Exit.
 */
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

/*
 * Function description:
 * - Build and show in explorer user download directory.
 */
static void
show_download_directory(void) {
	char dd[1024];

	build_download_directory(dd);
	ShellExecuteA(NULL, "open", dd, NULL, NULL, SW_SHOWNORMAL);

	return;
}

/*
 * Function description:
 * - Receive the http body which contains the desired theme (dark or light). Set
 *   a cookie in the user browser which hold wanted theme.
 * Arguments:
 * - httpnv: Contains "Content-Length" useful for the number of byte to receive.
 * - s_user: User socket wu read data from.
 * - theme: Is 0 or 1: dark theme or light theme
 * Return value:
 * - Return -1 if an error occurs and 0 if no errors occurs
 */
static int
handle_theme_change(struct header_nv* httpnv, int s_user, int* theme) {
	char cookie[48];

	if (wu_recv_theme(httpnv, s_user, theme) < 0)
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

/*
 * Function description:
 * - Transforms a requested resource in a response resource and send it back to the user.
 * Arguments:
 * - httpnv: Contains theme information.
 * - s_user: Client or user socket used for data transfer.
 * - bytesent: Used to store the total number of bytes wu send to the client.
 * - theme: Hold the theme informations: Is it dark or light.
 * - resource_index: Index used to retrieve local resource information. Index is used
 *   for searching and finding local resource information.
 * - cursorPosition: Here cursorPosition is the cursor where wu start showing error if occurs.
 */
static void
create_send_resource(struct header_nv* httpnv, int s_user, int* bytesent, int theme, int resource_index, COORD cursorPosition[2]) {
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

/*
 * Function description:
 * - Handle two "POST" request. One for changing theme and the other to upload a file.
 * Arguments:
 * - reqline: Contains the http resource string ("theme" or "upload")
 * - httpnv: User header names values. Among these there is "Content-Length" and
 *   "Cookie"
 * - s_user: Client socket where send the data to.
 * - bytesent: Total size of data the client are going to receive.
 * - theme: Variable indicating whether the theme to serv is light theme or dark theme.
 * - cursorPosition: User in clearing the screen pane.
 * Return value:
 * - Return -1 if an error occur and return 0 if no errors occur
 */
static int
handle_post_request(struct http_reqline* reqline, struct header_nv* httpnv, int s_user,
	int* bytesent, int theme, COORD cursorPosition[2]) {
	int err;

	if (strcmp(reqline->resource + 1, "theme") == 0) {
		err = handle_theme_change(httpnv, s_user, &theme);
		if (err < 0)
			return -1;
	}
	else if (strcmp(reqline->resource + 1, "upload") == 0) {
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

/*
 * Function description:
 * - Format a string according to http log format.
 * Arguments:
 * - logentry: Contains the formatted buffer ready to be written in log file.
 * - ipaddrstr: Client or user Internet Protocol address.
 * - reqline: The client or user http request line which contains http method and
 *   http resource and http version.
 * - bytesent: Total byte sent to user socket (http header and http body)
 * - status_code: Http "Ok" or "Bad Request" status code response.
 */
static void
create_log_entry(char* logentry, char* ipaddrstr, struct http_reqline* reqline, int bytesent, unsigned int status_code) {
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


/*
 * Function description:
 * Receive client request (request line and header pairs) and handle the request and reply if
 * the request is correct.
 * Arguments:
 * - cursorPosition: Position where to print error message or information.
 * - inaddr: IP address used to match against "Host" header field.
 * - s: Client or user socket.
 * - logentry: Used to store the http log entry.
 * Return value:
 * - 0: Success.
 */
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
	if (http_recv_reqline(s_user, &reqline) != 0)
		goto err;

	ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
	if (http_recv_headernv(s_user, httpnv) != 0)
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
		}
		else if (strcmp(reqline.resource + 1, "quit") == 0) {
			create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 200);
			wu_quit_response(cursorPosition, httpnv, &theme, s_user, &bytesent);
			quit_wu(s_user);
		}
		else if (strcmp(reqline.resource + 1, "openRep") == 0) {
			show_download_directory();

			if (strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/index") != 0)
				goto err;

			resource_index = 0;
		}

		create_send_resource(httpnv, s_user, &bytesent, theme, resource_index, cursorPosition);
		create_log_entry(logentry, ipaddrstr, &reqline, bytesent, 200);
	}
	else if (strcmp(reqline.method, "POST") == 0) {
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


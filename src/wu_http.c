#include <Windows.h>
#include <strsafe.h>
#include <time.h>

#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_msg.h"

#define HTTP_METHOD_GET    "GET"
#define HTTP_METHOD_POST   "POST"
#define HTTP_VERSION       "HTTP/1.1"

extern struct _http_resources http_resources[];
extern struct wu_msg wumsg[];
extern FILE* g_fplog;

/*
 * Function description:
 * - Format date to GMT format.
 * Arguments:
 * - http_date: Hold the formated date
 * Return value:
 * 1: Success.
 * -1: Failure.
 */
int
time_to_httpdate(char* http_date)
{
	struct tm intmv;
	time_t now;

	time(&now);

	ZeroMemory(&intmv, sizeof(struct tm));
	gmtime_s(&intmv, &now);

	ZeroMemory(http_date, HEADER_VALUE_MAX_SIZE);
	if (strftime((char*)http_date, HEADER_VALUE_MAX_SIZE,
		"%a, %d %b %Y %H:%M:%S GMT", &intmv) == 0)
		return -1;

	return 1;
}

/*
 * Function description:
 * - Receive HTTP/1.1 request line.
 * Arguments:
 * - s: Client socket wu receive data from.
 * - reqline: Used to store client request line.
 * Return value:
 * - -1: Function failure
 * - 0: Success
 */
int
http_recv_reqline(int s, struct http_reqline* reqline) {
	int i, ret;
	char c;

	i = 0;

	do {
		ret = recv(s, &c, 1, 0);
		if (ret < 0)
			return -1;
		else if (c == ' ')
			break;
		else
			reqline->method[i] = c;

	} while (++i < REQUEST_LINE_METHOD_MAX_SIZE);

	if (i == REQUEST_LINE_METHOD_MAX_SIZE || c != ' ')
		return -1;

	i = 0;

	do {
		ret = recv(s, &c, 1, 0);
		if (ret < 0)
			return -1;
		else if (c == ' ')
			break;
		else
			reqline->resource[i] = c;
	} while (++i < HTTP_RESSOURCE_MAX_LENGTH);

	if (i == HTTP_RESSOURCE_MAX_LENGTH || c != ' ')
		return -1;

	ret = recv(s, (char*) &reqline->version, sizeof("HTTP/1.1") - 1, 0);

	if (ret != (sizeof("HTTP/1.1") - 1))
		return -1;

	ret = recv(s, &c, 1, 0);
	if (ret < 0 || c != '\r')
		return -1;

	ret = recv(s, &c, 1, 0);
	if (ret < 0 || c != '\n')
		return -1;

	return 0;
}

/*
 * Function description:
 * - Receive http header request. Receive pairs of name/value split by ": "
 * Arguments:
 * - s: User socket wu receive data from.
 * - httpnv: Array to store pairs of name/value.
 * Return value:
 * 1: Function failure.
 * 0: Success.
 */
int
http_recv_headernv(int s, struct header_nv* httpnv) {
	int ret, nb_nv;
	int idx_name, idx_value;
	char c;

	for (nb_nv = 0; nb_nv < HEADER_NV_MAX_SIZE; nb_nv++) {
		idx_name = 0;
		do {
			ret = recv(s, &c, 1, 0);
			if (ret < 0)
				return -1;
			else if (c == ':')
				break;
			else if (c == '\r' && recv(s, &c, 1, 0) && c == '\n')
				return 0;
			else
				(httpnv + nb_nv)->name.client[idx_name] = c;
		} while (++idx_name < HEADER_NAME_MAX_SIZE);

		if (idx_name == HEADER_NAME_MAX_SIZE || c != ':')
			return -1;

		ret = recv(s, &c, 1, 0);
		if (ret < 0 || c != ' ')
			return -1;

		idx_value = 0;
		do {
			ret = recv(s, &c, 1, 0);
			if (ret < 0)
				return -1;
			else if (c == '\r')
				break;
			else
				(httpnv + nb_nv)->value.v[idx_value] = c;
		} while (++idx_value < HEADER_VALUE_MAX_SIZE);

		if (idx_value == HEADER_VALUE_MAX_SIZE || c != '\r')
			return -1;

		ret = recv(s, &c, 1, 0);
		if (ret < 0 || c != '\n')
			return -1;

	}

	return -1;
}

/*
 * Function description:
 * - Build http header pairs. Pairs are name/value strings.
 * Arguments:
 * - res: Contains the resource type (html or image or ...).
 * - nv: Http header names/values pairs.
 * - File size for "Content-Length" header field.
 */
static void
create_http_header_nv(struct http_resource* res, struct header_nv* nv, size_t fsize) {
	int i = 0;

	(nv + i)->name.wsite = HTTP_HEADER_CACHE_CONTROL;
	(nv + i++)->value.pv = HTTP_HEADER_CACHE_CONTROL_VALUE;

	(nv + i)->name.wsite = "Connection";
	(nv + i++)->value.pv = "Close";

	(nv + i)->name.wsite = HTTP_HEADER_CONTENT_LANGUAGE;
	(nv + i++)->value.pv = HTTP_HEADER_CONTENT_LANGUAGE_VALUE;

	(nv + i)->name.wsite = HTTP_HEADER_CONTENT_LENGTH;
	StringCchPrintfA((nv + i++)->value.v, HEADER_VALUE_MAX_SIZE, "%lu", fsize);

	(nv + i)->name.wsite = HTTP_HEADER_CONTENT_TYPE;
	strcpy_s((nv + i++)->value.v, HEADER_VALUE_MAX_SIZE, res->type);

	(nv + i)->name.wsite = HTTP_HEADER_DATE;
	time_to_httpdate((nv + i++)->value.v);

	/*
	(nv + i)->name.wsite = HTTP_HEADER_KEEP_ALIVE;
	(nv + i++)->value.pv = HTTP_HEADER_KEEP_ALIVE_VALUE;
	*/

	(nv + i)->name.wsite = HTTP_HEADER_SERVER;
	(nv + i++)->value.pv = HTTP_HEADER_SERVER_VALUE;

	return;
}

/*
 * Function description:
 * - Walk through nv array that contains pair of name value which shape
 *  http header.
 * Arguments:
 * - nv: Name value array.
 * - s: Socket.
 * - bytesent: Total byte sent.
 * Return value:
 * - 0: Success.
 * - 1: Failure.
 */
static int
send_http_header_nv(struct header_nv* nv, int s, int* bytesent) {
	int i;
	int ret = 0;

	for (i = 0; i < HEADER_NV_MAX_SIZE && (nv + i)->name.wsite != NULL; i++) {
		ret = send(s, (nv + i)->name.wsite, (int)strlen((nv + i)->name.wsite), 0);
		if (ret < 1)
			return 1;

		*bytesent += ret;

		ret = send(s, ": ", 2, 0);
		if (ret != 2)
			return 1;

		*bytesent += ret;

		if ((nv + i)->value.pv != NULL) {
			ret = send(s, (nv + i)->value.pv, (int)strlen((nv + i)->value.pv), 0);
			if (ret < 1)
				return 1;

			*bytesent += ret;
		}
		else {
			ret = send(s, (nv + i)->value.v, (int)strlen((nv + i)->value.v), 0);
			if (ret < 1)
				return 1;

			*bytesent += ret;
		}

		ret = send(s, "\r\n", 2, 0);

		*bytesent += ret;

		if (ret != 2)
			return 1;
	}

	send(s, "\r\n", 2, 0);

	*bytesent += ret;

	return 0;
}

/*
 * Function description:
 * - Send HTTP/1.1 reply status code. It can be 200 Ok or 404 Bad Request.
 * Arguments:
 * - s_user: User socket to send data to.
 * - bytesent: Total byte sent.
 * - status_code: Ok status or Bad Request status.
 * Return value:
 * - 0: Success.
 * - -1: Failure.
 */
static int
http_send_status(int s_user, int* bytesent, unsigned int status_code) {
	int ret;

	ret = send(s_user, HTTP_VERSION, sizeof(HTTP_VERSION) - 1, 0);
	if (ret != sizeof(HTTP_VERSION) - 1)
		return -1;

	*bytesent += ret;

	if (send(s_user, " ", 1, 0) != 1)
		return -1;

	*bytesent += ret;

	if (status_code == 404) {
		ret = send(s_user, HTTP_CODE_STATUS_BAD_REQUEST_STR,
			sizeof(HTTP_CODE_STATUS_BAD_REQUEST_STR) - 1, 0);
		if (ret != sizeof(HTTP_CODE_STATUS_BAD_REQUEST_STR) - 1)
			return -1;

		*bytesent += ret;

		if (send(s_user, " ", 1, 0) != 1)
			return -1;

		*bytesent += ret;

		ret = send(s_user, HTTP_STRING_STATUS_BAD_REQUEST,
			sizeof(HTTP_STRING_STATUS_BAD_REQUEST) - 1, 0);

		*bytesent += ret;
	}
	else {
		ret = send(s_user, HTTP_CODE_STATUS_OK_STR, sizeof(HTTP_CODE_STATUS_OK_STR) - 1, 0);
		if (ret != sizeof(HTTP_CODE_STATUS_OK_STR) - 1)
			return -1;

		*bytesent += ret;

		if (send(s_user, " ", 1, 0) != 1)
			return -1;

		*bytesent += ret;

		ret = send(s_user, HTTP_STRING_STATUS_OK, sizeof(HTTP_STRING_STATUS_OK) - 1, 0);
		if (ret != sizeof(HTTP_STRING_STATUS_OK) - 1)
			return -1;

		*bytesent += ret;
	}

	ret = send(s_user, "\r\n", 2, 0);
	if (ret != 2)
		return -1;

	*bytesent += ret;

	return 0;
}

/*
 * Function description:
 * - Get current time and format it to store it in a buffer.
 * Arguments:
 * - hrmn: Hours minutes characters string.
 */
static
int get_hours_minutes(char hrmn[6]) {
	SYSTEMTIME sysTime;

	ZeroMemory(&sysTime, sizeof(SYSTEMTIME));
	GetLocalTime(&sysTime);
	ZeroMemory(hrmn, 6);
	if (sysTime.wHour < 10 && sysTime.wMinute < 10)
		StringCchPrintf(hrmn, 6, "0%hu:0%hu", sysTime.wHour, sysTime.wMinute);
	else if (sysTime.wHour < 10)
		StringCchPrintf(hrmn, 6, "0%hu:%hu", sysTime.wHour, sysTime.wMinute);
	else if (sysTime.wMinute < 10)
		StringCchPrintf(hrmn, 6, "%hu:0%hu", sysTime.wHour, sysTime.wMinute);
	else
		StringCchPrintf(hrmn, 6, "%hu:%hu", sysTime.wHour, sysTime.wMinute);

	return 0;
}

/*
 * Function description
 * - Format html data with input variables. Store the formated html data in
 *   pbufferout new buffer.
 * Arguments:
 * - successinfo: Hold tx statistics informations.
 * - resource: Resource string name to validate.
 * - pbufferin: No-formated html data page.
 * - pbufferout: Buffer that will hold formated html data page.
 * - pbufferoutlen: Length of data in pbufferout.
 * - fsize: File size.
 * - hrmn: Hours and minutes for current time to display.
 * Return value:
 * - -1: Function failed
 * - 0: Function success.
 */
static
int make_htmlpage(struct success_info* successinfo, char* resource, char* pbufferin,
	char** pbufferout, size_t* pbufferoutlen, DWORD fsize, char hrmn[6]) {
	char BufferUserName[254];
	DWORD BufferUserNameSize = 254;

	ZeroMemory(BufferUserName, 254);
	GetUserNameA(BufferUserName, &BufferUserNameSize);

	if (strcmp(resource, "index") == 0 || strcmp(resource, "erreur_fichier_nul") == 0) {
		*pbufferout = (char*)malloc(fsize + BufferUserNameSize + 6);
		ZeroMemory(*pbufferout, fsize + BufferUserNameSize + 6);
		StringCchPrintfA(*pbufferout, fsize + BufferUserNameSize + 6 - 1, pbufferin, BufferUserName, hrmn);
		free(pbufferin);
		*pbufferoutlen = strlen(*pbufferout);
	}
	else if (strcmp(resource, "success") == 0) {
		*pbufferout = (char*)malloc(fsize + 254 + 6);
		ZeroMemory(*pbufferout, fsize + 254 + 6);
		StringCchPrintfA(*pbufferout, fsize + 254 + 6,
			pbufferin,
			successinfo->filename,
			successinfo->filenameSize,
			successinfo->elapsedTime,
			successinfo->averagespeed,
			hrmn);
		free(pbufferin);
		*pbufferoutlen = strlen(*pbufferout);
	}
	else if (strcmp(resource, "credits") == 0 || strcmp(resource, "erreur_404") == 0 || strcmp(resource, "settings") == 0) {
		*pbufferout = (char*)malloc(fsize + 6);
		ZeroMemory(*pbufferout, fsize + 6);
		StringCchPrintfA(*pbufferout, fsize + 6, pbufferin, hrmn);
		free(pbufferin);
		*pbufferoutlen = strlen(*pbufferout);
	}
	else {
		free(pbufferin);
		return -1;
	}

	return 0;
}

/*
 * Function description:
 * - Send http header and send html body content.
 * Arguments:
 * - s_user: User socket.
 * - httpnv: Prepared http header names values.
 * - bytesent: Total byte sent.
 * - pbufferout: Formated html content.
 * - pbufferoutlen: Formated html content length.
 * - hFile: Handle to be closed if an error occurs.
 * Return value:
 * - 0: Success.
 * - 1: send_http_header_nv failed.
 * - 2: Send body failed
 */
static int
send_http_header_html_content(int s_user, struct header_nv* httpnv, int* bytesent,
	char* pbufferout, size_t pbufferoutlen, HANDLE hFile) {
	int ret = 0;

	ret = send_http_header_nv(httpnv, s_user, bytesent);
	if (ret > 0) {
		if (pbufferout)
			free(pbufferout);
		CloseHandle(hFile);
		return 1;
	}

	ret = send(s_user, pbufferout, (int)pbufferoutlen, 0);
	if (ret <= 0) {
		free(pbufferout);
		CloseHandle(hFile);
		return 2;
	}


	*bytesent += ret;

	return 0;
}

/*
 * Function description:
 * - Send http header names values ("Content-Length" "Content-Type" ...) next
 *   send image http body data.
 * Arguments:
 * - hFile: Handle that contains the file to read and send to user socket.
 * - s_user: User socket.
 * - httpnv: Header names values to send.
 * - bytesent: Total byte sent.
 * Return value:
 * - 0: Success
 * - 1: Failure
 */
static int
send_http_header_image_file(HANDLE hFile, int s_user, struct header_nv* httpnv, int* bytesent) {
	int ret = 0;
	char buffer[1024];
	DWORD read;

	ret = send_http_header_nv(httpnv, s_user, bytesent);
	if (ret > 0) {
		CloseHandle(hFile);
		return 1;
	}

	*bytesent += ret;

	while (ReadFile(hFile, buffer, 1024, &read, NULL)) {
		ret = send(s_user, buffer, read, 0);

		*bytesent += ret;

		if (read < 1024)
			break;
	}

	return 0;
}


/*
 * Function description:
 * - Load data, format html data if required and send http and body data.
 * Arguments:
 * - res: Resource to send.
 * - s: User socket where send data.
 * - successinfo: Structure that contains transfer statistics.
 * - bytesent: Total byte wu send to the user socket.
 * - status_code: HTTP status code response.
 */
int
http_serv_resource(struct http_resource* res, int s,
	struct success_info* successinfo,
	int* bytesent, unsigned int status_code) {
	HANDLE hFile;
	DWORD fsize, read, err;
	struct header_nv httpnv[HEADER_NV_MAX_SIZE];
	char* pbufferin = NULL, * pbufferout = NULL;
	char BufferUserName[254];
	size_t pbufferoutlen = 0;
	char hrmn[6];
	char* plastBS;
	int ret;


	if (http_send_status(s, bytesent, status_code) < 0)
		return -1;

	hFile = CreateFile(res->resource, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return GetLastError();

	fsize = GetFileSize(hFile, NULL);

	if (strcmp(res->type, "text/html") == 0) {
		pbufferin = (char*)malloc(fsize + 1);

		ZeroMemory(pbufferin, fsize + 1);
		ReadFile(hFile, pbufferin, fsize, &read, NULL);
		if (read != fsize) {
			err = GetLastError();
			free(pbufferin);
			CloseHandle(hFile);
			return err;
		}

		get_hours_minutes(hrmn);

		plastBS = strrchr(res->resource, '\\') + 1;
		if (make_htmlpage(successinfo, plastBS, pbufferin, &pbufferout,
			&pbufferoutlen, fsize, hrmn) < 0)
			goto err;

		ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		create_http_header_nv(res, httpnv, pbufferoutlen);

		if (send_http_header_html_content(s, httpnv, bytesent, pbufferout,
			pbufferoutlen, hFile) != 0)
			goto err;

		free(pbufferout);
	err:
		CloseHandle(hFile);
	}
	else if (strcmp(res->type, "image/png") == 0 || strcmp(res->type, "image/x-icon") == 0) {
		char buffer[1024];

		ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		create_http_header_nv(res, httpnv, fsize);

		send_http_header_image_file(hFile, s, httpnv, bytesent);

		CloseHandle(hFile);
		ret = 1;
	}

	return ret;
}

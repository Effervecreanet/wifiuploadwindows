#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_tls_https.h"
#include "wu_http_nv.h"
#include "wu_msg.h"
#include "wu_content.h"

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE* g_fphttpslog;
extern int g_tls_firstsend;
extern const struct _http_resources http_resources[];


/*
 * Function description:
 *  - Parse client request line. Get http method, http ressource and http
 * version.
 * Arguments:
 *  - reqline: Structure to store user http method, ressource and version.
 *  - BufferIn: tls_recv() output buffer
 *  - length: Length of BufferIn.
 * Return value:
 *  - -1: Failure
 *  - 0: Number of data characters parsed. 
 */
int get_request_line(struct http_reqline* reqline, char* BufferIn, int length)
{
	char* p_bufferin = BufferIn;
	int i = 0, count = 0;

	if (length == 0)
		return -1;

	do {
		reqline->method[i++] = *p_bufferin++;
		if (*p_bufferin == ' ')
			break;
	} while (i <= REQUEST_LINE_METHOD_MAX_SIZE);

	if (*p_bufferin != ' ')
		return -1;
	p_bufferin++;

	count += i;
	i = 0;

	do {
		reqline->resource[i++] = *p_bufferin++;
		if (*p_bufferin == ' ')
			break;
	} while (i <= HTTP_RESSOURCE_MAX_LENGTH);

	if (*p_bufferin != ' ')
		return -1;
	p_bufferin++;

	count += i;
	i = 0;

	do {
		reqline->version[i++] = *p_bufferin++;
		if (*p_bufferin == '\r')
			break;
	} while (i <= sizeof(HTTP_VERSION) - 1);

	if (*p_bufferin != '\r')
		return -1;

	if (strcmp(reqline->version, HTTP_VERSION) != 0)
		return -1;

	count += i;
	count += 4; /* Add ignored characters count and skip "\r\n" */

	return count;
}

/*
 * Function description:
 *  - Parse user http header name/value pairs. Store http header name/value
 *    pairs in a struct header_nv.
 * Arguments:
 *  - headernv: Structure that contains http header name/value pairs.
 *  - buffer: tls_recv() output buffer specified with an offset
 *            (+= length of request line).
 *  - bufferlength: Length of buffer.
 * Return value:
 *  -1: Function failure. NAME or VALUE size out of bounds. Or invalid characters.
 *   0: Number of byte parsed.
 */
int get_header_nv(struct header_nv headernv[HEADER_NV_MAX_SIZE], char* buffer, int bufferlength, struct in_addr inaddr)
{
	int count_hdr_nv = 0;
	int headerlen = 0;
	int i;

	if (bufferlength == 0)
		return -1;

	do {

		i = 0;

		do {
			if (isalpha(*buffer) != 0 || *buffer == '-' || *buffer == '.')
				headernv[count_hdr_nv].name.client[i++] = *buffer++;
			else if (*buffer == ':' && *(buffer + 1) == ' ')
				break;
			else if (*buffer == '\r' && *(buffer + 1) == '\n')
				goto crlfcrlf;
			if (headerlen++ >= HEADER_MAX_SIZE)
				return -1;
		} while (i < HEADER_NAME_MAX_SIZE);

		if (i == HEADER_NAME_MAX_SIZE)
			return -1;

		buffer += 2;
		headerlen += 2;

		if (headerlen >= HEADER_MAX_SIZE)
			return -1;

		i = 0;

		do {
			headernv[count_hdr_nv].value.v[i++] = *buffer++;
			if (*buffer == '\r' && *(buffer + 1) == '\n')
				break;
			if (headerlen++ >= HEADER_MAX_SIZE)
				return -1;
		} while (i < HEADER_VALUE_MAX_SIZE);

		if (i == HEADER_VALUE_MAX_SIZE)
			return -1;

		buffer += 2;
		headerlen += 3;

		if (headerlen >= HEADER_MAX_SIZE)
			return -1;
	} while (++count_hdr_nv < HEADER_NV_MAX_SIZE);

crlfcrlf:
	buffer += 2;
	headerlen += 2;

	if (headerlen >= HEADER_MAX_SIZE)
		return -1;

	i = nv_find_name_client(headernv, "Host");
	if (i < 0 || strcmp(headernv[i].value.v, inet_ntoa(inaddr)) != 0)
		return -1;

	return headerlen;
}

int
get_https_request(CtxtHandle* ctxtHandle, int s_clt, char** tls_recv_output, unsigned int* tls_recv_output_size, COORD* cursorPosition,
	struct http_reqline* reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE], struct in_addr inaddr) {
	int  header_offset;
	int ret;

	ret = tls_recv(ctxtHandle, s_clt, tls_recv_output, tls_recv_output_size, cursorPosition);
	if (ret < 0)
		return -1;

	ZeroMemory(reqline, sizeof(struct http_reqline));
	ret = get_request_line(reqline, *tls_recv_output, *tls_recv_output_size);
	if (ret < 0)
		return -1;
	else
		header_offset = ret;

	ZeroMemory(headernv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
	ret = get_header_nv(headernv, *tls_recv_output + header_offset, *tls_recv_output_size - header_offset, inaddr);
	if (ret < 0)
		return -1;
	else
		header_offset += ret;

	return header_offset;
}

int
handle_get_request(CtxtHandle* ctxtHandle, int s_clt, struct http_reqline *reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE],
					int* bytesent, COORD *cursorPosition) {
	struct http_resource httplocalres;
	int theme;
	int ires;

	ires = http_match_resource((char*)&reqline->resource);
	if (ires < 0) {
		check_cookie_theme(headernv, &theme);

		for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

		ZeroMemory(&httplocalres, sizeof(struct http_resource));
		if (create_local_resource(&httplocalres, ires, theme) != 0)
			show_error_wait_close(cursorPosition, ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

		https_serv_resource(&httplocalres, s_clt, NULL, bytesent, ctxtHandle, cursorPosition);
	}
	else if (strcmp(reqline->resource + 1, "quit") == 0) {
		https_wu_quit_response(cursorPosition, headernv, &theme, s_clt, bytesent);
		https_quit_wu(s_clt);
	}
	else if (strcmp(reqline->resource + 1, "openRep") == 0) {
		show_download_directory();
		strcpy_s(reqline->resource, HTTP_RESSOURCE_MAX_LENGTH, "/index");
		ires = 0;
		goto after_openrep;
	}
	else {
after_openrep:
		check_cookie_theme(headernv, &theme);

		ZeroMemory(&httplocalres, sizeof(struct http_resource));
		if (create_local_resource(&httplocalres, ires, theme) != 0)
			show_error_wait_close(cursorPosition, ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

		https_serv_resource(&httplocalres, s_clt, NULL, bytesent, ctxtHandle, cursorPosition);
	}

	return 0;
}

int handle_post_request(CtxtHandle* ctxtHandle, int s_clt, struct http_reqline* reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE],
					int* bytesent, COORD* cursorPosition) {
	int theme;
	int ret;

	if (strcmp(reqline->resource, "/theme") == 0) {
		char cookie[48];
		check_cookie_theme(headernv, &theme);
		if (theme == 0)
			strcpy_s(cookie, 48, "theme=dark");
		else
			strcpy_s(cookie, 48, "theme=light");
		https_apply_theme(s_clt, ctxtHandle, cookie, *cursorPosition);
	}
	else if (strcmp(reqline->resource, "/upload") == 0) {
		struct user_stats upstats;

		clear_txrx_pane(cursorPosition);
		check_cookie_theme(headernv, &theme);
		ZeroMemory(&upstats, sizeof(struct user_stats));
		ret = tls_receive_file(cursorPosition, headernv, s_clt, &upstats, theme, bytesent, ctxtHandle);
		if (ret < 0)
			return -1;
		cursorPosition->X = (cursorPosition + 1)->X;
		cursorPosition->Y++;
	}

	return 0;
}
/*
 * Function description:
 *  - Load wifiupload html page or image data, format html page or image data
 *    eventualy send data.
 * Arguments:
 *  - res: Wifiupload requested ressource. See wu_content.c it contains wifiupload
 *         ressources (pages and images ...).
 *  - s: Client socket where wu send data.
 *  - successinfo: Upload transfert informations and statistics.
 *  - bytesent: Number of byte wifiupload sent to client.
 *  - ctxtHandle: Security context handle, used in tls_send().
 *  - cursorPosition: Console cursor position where wu writes info or errors.
 * Return value:
 *  - Not used. 
 */

int
https_serv_resource(struct http_resource* res, int s,
	struct success_info* successinfo,
	int* bytesent, CtxtHandle* ctxtHandle,
	COORD cursorPosition) {
	HANDLE hFile;
	DWORD fsize, read, err;
	DWORD BufferUserNameSize = 254;
	SYSTEMTIME sysTime;
	struct header_nv httpnv[HEADER_NV_MAX_SIZE];
	char* pbufferin = NULL, * pbufferout = NULL;
	char BufferUserName[254];
	size_t pbufferoutlen = 0;
	char hrmn[6];
	char* plastBS;
	int ret;
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[4];
	int i;
	char message[8192];
	int encryptBufferLen;
	int messageLen;

	ZeroMemory(message, 8192);
	sprintf_s(message, 8192, "%s %s %s\r\n", HTTP_VERSION, HTTP_CODE_STATUS_OK_STR, HTTP_STRING_STATUS_OK);

	hFile = CreateFile(res->resource, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return GetLastError();

	fsize = GetFileSize(hFile, NULL);

	if (strcmp(res->type, "text/html") == 0) {
		ZeroMemory(BufferUserName, 254);
		GetUserNameA(BufferUserName, &BufferUserNameSize);

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
		create_http_header_nv(res, httpnv, pbufferoutlen, -1);

		for (i = 0; i < HEADER_NV_MAX_SIZE && httpnv[i].name.wsite != NULL; i++) {
			messageLen = strlen(message);
			sprintf_s(message + messageLen, 8192 - messageLen,
				"%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, 8192 - messageLen, "\r\n");
		strcat_s(message, 8192 - messageLen - 2, pbufferout);

		tls_send(s, ctxtHandle, message, strlen(message), cursorPosition);

		*bytesent += messageLen;

		ret = 0;
		free(pbufferout);
	err:
		CloseHandle(hFile);
	}
	else if (strcmp(res->type, "image/png") == 0 || strcmp(res->type, "image/x-icon") == 0) {
		char buffer[1024];

		ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		create_http_header_nv(res, httpnv, fsize, -1);

		for (i = 0; i < HEADER_NV_MAX_SIZE && httpnv[i].name.wsite != NULL; i++) {
			messageLen = strlen(message);
			sprintf_s(message + messageLen, 8192 - messageLen, "%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, 8192 - messageLen, "\r\n");

		messageLen = strlen(message);

		*bytesent += strlen(message);

		tls_send(s, ctxtHandle, message, strlen(message), cursorPosition);

		while (ReadFile(hFile, message, 2048, &read, NULL) != 0 && read) {
			tls_send(s, ctxtHandle, message, read, cursorPosition);

			*bytesent += read;
		}

		CloseHandle(hFile);
		ret = 1;
	}

	return ret;
}


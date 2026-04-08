#include <Windows.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_txstats.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_tls_conn.h"


extern FILE *g_fphttpslog;

static void wu_https_post_theme_hdr_nv(struct header_nv hdrnv[32], char* cookie);


/*
 * Function description:
 * - Build header pairs name/value with "Set-Cookie" which contains theme. Header HTTP_HEADER_LOCATION
 *   is set to "/" to redirect user to home page after theme change.
 * Arguments:
 * - hdrnv: Array of header pairs.
 * - cookie: Contains theme information ("light" or "dark")
 */

static void
wu_https_post_theme_hdr_nv(struct header_nv hdrnv[32], char* cookie)
{
	int cnt = 0;
	time_t now = time(NULL);
	struct tm tmv;
	char expires[HTTP_STRING_DATE_SIZE];

	hdrnv[cnt].name.wsite = HTTP_HEADER_CACHE_CONTROL;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CACHE_CONTROL_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONNECTION;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CONNECTION_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_SERVER;
	hdrnv[cnt++].value.pv = HTTP_HEADER_SERVER_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONTENT_LENGTH;
	strcpy_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE, "0");

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONTENT_LANGUAGE;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CONTENT_LANGUAGE_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_SET_COOKIE;

	ZeroMemory(&tmv, sizeof(struct tm));
	gmtime_s(&tmv, &now);
	tmv.tm_year++;
	strftime(expires, HTTP_STRING_DATE_SIZE, "%a, %d %b %Y %H:%M:%S GMT", &tmv);

	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, cookie);
	strcat_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, "; Expires=");
	strcat_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE, expires);

	hdrnv[cnt].name.wsite = HTTP_HEADER_LOCATION;
	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, "/");

	return;
}


/*
 * Function description:
 * - Send back header pairs of name/value to apply theme change and redirect user to home page.
 * Arguments:
 * - s_clt: User socket to send message to.
 * - ctxtHandle: Security context handle, used in tls_send() function.
 * - cookie: Contains theme information ("light" or "dark") to build header pairs.
 * - cursorPosition: Console cursor position where wu writes schannel error if any.
 * Return value:
 * -1: Function failed (tls_send() failed).
 * 0: Function succeeded.
 */

int
https_apply_theme(SOCKET s_clt, CtxtHandle *ctxtHandle, char* cookie, COORD cursorPosition)
{
	struct header_nv hdrnv[32];
	char buffer[2048];
	int i;

	memset(hdrnv, 0, sizeof(struct header_nv) * 32);
	wu_https_post_theme_hdr_nv(hdrnv, cookie);

	ZeroMemory(buffer, 2048);
	strcpy_s(buffer, 2048, "HTTP/1.1 301 Moved Permanently\r\n");

	for (i = 0; i < 32; i++) {
		if (hdrnv[i].name.wsite == NULL)
			break;

		strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].name.wsite);
		strcat_s(buffer, 2048 - strlen(buffer), ": ");

		if (hdrnv[i].value.pv != NULL)
			strcat_s(buffer, 2048, hdrnv[i].value.pv);
		else
			strcat_s(buffer, 2048, hdrnv[i].value.v);

		strcat_s(buffer, 2048, "\r\n");

	}

	strcat_s(buffer, 2048 - strlen(buffer), "\r\n");

	if (tls_send(s_clt, ctxtHandle, buffer, (unsigned int)strlen(buffer), cursorPosition) < 0)
		return -1;

	return 0;
}


/*
 * Function description:
 * - Parse http body to get theme parameter the user spplied in POST request to change theme.
 * Arguments:
 * - headernv: Structure that contains http header name/value pairs. It is used to get "Content-Length" header value.
 * - bufferIn: Buffer that contains http body data.
 * - theme: Pointer to int to update with theme value. It is set to 1 if theme is "light" and set to 0 if theme is "dark".
 * Return value:
 * - -1: Function failed. It can be caused by "Content-Length" header not found or if theme parameter is not "light" or "dark".
 * - 0: Function succeeded.
 */

int
get_theme_param(struct header_nv *headernv, char *bufferIn, int *theme) {
	int idxclen;
	int clen;

	idxclen = nv_find_name_client(headernv, "Content-Length");
	if (idxclen < 0) 
		return -1;

	clen = atoi((headernv + idxclen)->value.v);

	if (strncmp(bufferIn, "theme=light", clen) == 0)
		*theme = 1;
	else if (strncmp(bufferIn, "theme=dark", clen) == 0)
		*theme = 0;
	else
		return -1;

	return 0;
}

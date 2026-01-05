#include <Windows.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_nv.h"

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE *g_fphttpslog;

void
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

int
https_apply_theme(int s_clt, CtxtHandle *ctxtHandle, char* cookie, COORD cursorPosition)
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
			strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].value.pv);
		else
			strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].value.v);

		strcat_s(buffer, 2048 - strlen(buffer), "\r\n");

	}

	strcat_s(buffer, 2048 - strlen(buffer), "\r\n");

	if (tls_send(s_clt, ctxtHandle, buffer, strlen(buffer), cursorPosition) < 0)
		return -1;

	return 0;
}
int
get_theme_param(struct header_nv *headernv, char *bufferIn, int *theme) {
	int idxclen;
	int clen;
	int res;

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

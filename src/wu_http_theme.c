#include <Windows.h>
#include <time.h>
#include <stdio.h>

#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"


void
wu_http_post_theme_hdr_nv(struct header_nv hdrnv[32], char *cookie)
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
	strcpy_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE,"0");

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONTENT_LANGUAGE;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CONTENT_LANGUAGE_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_SET_COOKIE;
		
	ZeroMemory(&tmv, sizeof(struct tm));
	gmtime_s(&tmv, &now);
	tmv.tm_year++;
	strftime(expires, HTTP_STRING_DATE_SIZE, "%a, %d %b %Y %H:%M:%S GMT", &tmv);

	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, cookie);
	strcat_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE,"; Expires=");
	strcat_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE, expires);

	hdrnv[cnt].name.wsite = HTTP_HEADER_LOCATION;
	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, "/");

	return;
}

int
wu_http_recv_theme(struct header_nv httpnv[HEADER_NV_MAX_SIZE], int s_user, int *theme)
{
      int clen;
      int idxclen, i;
      char buffer[sizeof("theme=light")];

      idxclen = nv_find_name_client(httpnv, "Content-Length");
      if (idxclen < 0)
        return -1;

      ZeroMemory(buffer, sizeof("theme=light"));
      clen = atoi(httpnv[idxclen].value.v);
      for (i = 0; i < clen; i++) {
        if (recv(s_user, &buffer[i], 1, 0) != 1)
          break;
      }

      if (strcmp(buffer, "theme=dark") == 0) {
        *theme = 0;
      } else if (strcmp(buffer, "theme=light") == 0) {
        *theme = 1;
      } else {
        return -1;
      }

    return 0;
}

void
wu_http_send_hdr(int susr, struct header_nv hdrnv[32])
{
	int cnt = 0;

	for (cnt = 0; hdrnv[cnt].name.wsite != NULL; cnt++) {
		send(susr, hdrnv[cnt].name.wsite, (int) strlen(hdrnv[cnt].name.wsite), 0);
		send(susr, ": ", 2, 0);
		if (hdrnv[cnt].value.pv && *hdrnv[cnt].value.pv)
			send(susr, hdrnv[cnt].value.pv, (int) strlen(hdrnv[cnt].value.pv), 0);
		else
			send(susr, hdrnv[cnt].value.v, (int) strlen(hdrnv[cnt].value.v), 0);
		send(susr, "\r\n", 2, 0);
	}

	send(susr, "\r\n", 2, 0);

	return;
}

int
apply_theme(int susr, char *cookie)
{
	struct header_nv hdrnv[32];

	memset(hdrnv, 0, sizeof(struct header_nv) * 32);
	wu_http_post_theme_hdr_nv(hdrnv, cookie);

	if (send(susr, "HTTP/1.1 301 Moved Permanently\r\n", 31, 0) <= 0)
		return -1;
	
	wu_http_send_hdr(susr, hdrnv);

	return 1;
}

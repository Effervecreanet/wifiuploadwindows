#include <Windows.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"

#define HTTP_METHOD_GET  "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_VERSION 	 "HTTP/1.1"

int get_request_line(struct http_reqline *reqline, char *BufferIn)
{
	char *p, *psav, *sp;
	int i;

	for (i = 0, psav = p = BufferIn; i < 254; p++, i++)
		if (*p == '\r' && *(p + 1) == '\n')
			break;

	if (i == 254)
		return -1;

	*p = '\0';

	if ((sp = strchr(psav, ' ')) == NULL)
		return -1;

	*sp = '\0';
	strcpy_s(reqline->method, sizeof(HTTP_METHOD_POST), psav);

	psav = ++sp;
	if ((sp = strchr(psav, ' ')) == NULL)
		return -1;

	*sp = '\0';
	strcpy_s(reqline->resource, sizeof(HTTP_RESSOURCE_MAX_LENGTH), psav);
	strcpy_s(reqline->version, sizeof(HTTP_VERSION), sp + 1);

	return 0;
}

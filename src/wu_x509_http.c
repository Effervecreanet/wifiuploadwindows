#include <Windows.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"

int get_reqline(struct http_reqline *reqline, char *BufferIn)
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
	strcpy(reqline->method, psav);

	psav = ++sp;
	if ((sp = strchr(psav, ' ')) == NULL)
		return -1;

	*sp = '\0';
	strcpy(reqline->resource, psav);
	strcpy(reqline->version, sp + 1);

	return 0;
}

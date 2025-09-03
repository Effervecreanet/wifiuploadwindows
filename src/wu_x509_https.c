#include <Windows.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_x509_https.h"
#include "wu_http_nv.h"

int get_request_line(struct http_reqline *reqline, char *BufferIn)
{
	char *p, *psav, *sp;
	int i;

	for (i = 0, psav = p = BufferIn; i < 254; p++, i++)
		if (*p == '\r' && *(p + 1) == '\n')
			break;

	if (i >= 252)
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
	strcpy_s(reqline->resource, HTTP_RESSOURCE_MAX_LENGTH, psav);
	strcpy_s(reqline->version, sizeof(HTTP_VERSION), sp + 1);

	return i + 2;
}

int get_headernv(struct header_nv headernv[HEADER_NV_MAX_SIZE], char *buffer)
{
  int count;
  int count2 = 0;
  int maxsp;
  int hlen = 0;
  int nvnb = 0;

  do {
    for (count = 0; count < HEADER_NAME_MAX_SIZE && *(buffer + count2) != '\0'; count2++, count++) {
      if (isalpha(*(buffer + count2)) != 0 ||
		  *(buffer + count2) == '-' ||
		  *(buffer + count2) == '.') {
        headernv[nvnb].name.client[count] = *(buffer + count2);
        continue;
      }
      switch (*(buffer + count2)) {
      case ':':
        break;
      case '\n':
        goto crlfcrlf;
      case '\r':
        if (*(buffer + count2 + 1) == '\n')
          goto crlfcrlf;
        return -1;
      default:
        return -1;
      }
      break;
    }

    if (*(buffer + count2) == '\0' || count == HEADER_NAME_MAX_SIZE)
      return -1;

    if (*(buffer + ++count2) != ' ' && *(buffer + count2) != '\t')
      return -1;

    for (maxsp = 0; *(buffer + ++count2) != '\0' && maxsp < MAX_SP; ++maxsp) {
      switch (*(buffer + count2)) {
      case ' ':
      case '\t':
        continue;
      default:
        headernv[nvnb].value.v[0] = *(buffer + count2);
        break;
      }
      break;
    }

    if (*(buffer + count2) == '\0' || maxsp == MAX_SP)
      return -1;

    for (count = 1; *(buffer + ++count2) != '\0' && count < HEADER_VALUE_MAX_SIZE; count++) {
      switch (*(buffer + count2)) {
      case '\r':
      case '\n':
        break;
      default:
        headernv[nvnb].value.v[count] = *(buffer + count2);
        continue;
      }
      break;
    }

    if (*(buffer + count2) == '\0' || count == HEADER_VALUE_MAX_SIZE)
      return -1;

    if (*(buffer + count2) == '\r' && *(buffer + ++count2) != '\n')
      return -1;
    /*
       printf("header_name: %s header_value: %s\n",
       hdr_nv[nvnb].name.client, hdr_nv[nvnb].value.v);
     */
	++count2;
  } while (++nvnb < HEADER_NV_MAX_SIZE);

crlfcrlf:
  if (nvnb == HEADER_NV_MAX_SIZE)
    return -1;

  return 1;
}


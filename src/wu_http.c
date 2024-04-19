#include <Windows.h>

#include "wu_http.h"
#include "wu_http_nv.c"

#define HTTP_METHOD_GET    "GET"
#define HTTP_METHOD_POST   "POST"
#define HTTP_VERSION       "HTTP/1.1"

errno_t
http_recv_reqline(struct http_reqline* reqline, int s) {
  int i;
  char buffer[254];
  char* sp;
  char* spbak;
  errno_t errn;

  ZeroMemory(buffer, 254);
  for (i = 0; i < 252; i++) {
    recv(s, &buffer[i], 1, 0);
    if (buffer[i] == '\r') {
      recv(s, &buffer[++i], 1, 0);
      if (buffer[i] == '\n')
        break;
      else
        return -1;
    }
  }

  if (i >= 252)
    return -2;

  if ((sp = strchr(buffer, ' ')) == NULL)
    return 1;
  
  spbak = sp + 1;
  *sp = '\0';

  errn = strcpy_s(reqline->method, sizeof(HTTP_METHOD_POST), buffer);
  if (errn != 0)
    return EINVAL;

  if ((sp = strchr(++sp, ' ')) == NULL)
    return 3;

  *sp = '\0';

  errn = strcpy_s(reqline->resource, HTTP_RESSOURCE_MAX_LENGTH, spbak);
  if (errn != 0)
    return EINVAL;

  if (strncmp(++sp, HTTP_VERSION, sizeof(HTTP_VERSION) - 1))
    return 4;

  errn = strcpy_s(reqline->version, sizeof(HTTP_VERSION), HTTP_VERSION);
  if (errn != 0)
    return EINVAL;
  
  return 0;
}


errno_t
http_recv_headernv(struct header_nv* httpnv, int s) {
  CHAR buffer[HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE + 2];
  char *colon;
  int i, j;

  for (i = 0; i < HEADER_NV_MAX_SIZE; i++) {
    ZeroMemory(buffer, HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE + 2);
    for (j = 0; j < HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE + 2; j++) {
      if (recv(s, &buffer[j], 1, 0) != 1)
        return 10;

      if (buffer[j] == '\r') {
        if (recv(s, &buffer[++j], 1, 0) != 1)
          return 10;

        if (buffer[j] == '\n')
          break;
        else
          return 1;
      }
    }

    if (buffer[0] == '\r' && buffer[1] == '\n')
      break;

    if (j >= HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE + 2 || j <= 1)
      return 2;

    buffer[j] = '\0';
    buffer[j - 1] = '\0';

    if ((colon = strchr(buffer, ':')) == NULL || (*(colon + 1) && *(colon + 1) != ' '))
      return 3;

    *colon = '\0';


    if (strcpy_s((httpnv + i)->name.client, HEADER_NAME_MAX_SIZE, buffer) != 0)
      return EINVAL;

    colon += 2;
    
    if (strcpy_s((httpnv + i)->value.v, HEADER_VALUE_MAX_SIZE, colon) != 0)
      return EINVAL;
  }

  return 0;
}
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

static void create_http_header_nv(struct http_resource *res,
                                  struct header_nv *nv,
                                  size_t fsize);

static int send_http_header_nv(struct header_nv* nv,
                               int s);

static int
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

static void
create_http_header_nv(struct http_resource *res, struct header_nv *nv, size_t fsize) {
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

static int
send_http_header_nv(struct header_nv* nv, int s) {
  int i;
  int ret;

  for (i = 0; i < HEADER_NV_MAX_SIZE && (nv + i)->name.wsite != NULL; i++) {
    ret = send(s, (nv + i)->name.wsite, (int)strlen((nv + i)->name.wsite), 0);
    if (ret < 1)
      return 1;
    
    ret = send(s, ": ", 2, 0);
    if (ret != 2)
      return 2;

    if ((nv + i)->value.pv != NULL) {
      ret = send(s, (nv + i)->value.pv, (int)strlen((nv + i)->value.pv), 0);
      if (ret < 1)
        return 3;
    }
    else {
      ret = send(s, (nv + i)->value.v, (int)strlen((nv + i)->value.v), 0);
      if (ret < 1)
        return 3;
    }

    ret = send(s, "\r\n", 2, 0);
    if (ret != 2)
      return 4;
  }

  send(s, "\r\n", 2, 0);

  return 0;
}
  

int
http_serv_resource(struct http_resource *res, int s,
                   struct success_info *successinfo) {
  HANDLE hFile;
  DWORD fsize, read, err;
  DWORD BufferUserNameSize = 254;
  SYSTEMTIME sysTime;
  struct header_nv httpnv[HEADER_NV_MAX_SIZE];
  char *pbufferin = NULL, *pbufferout = NULL;
  char BufferUserName[254];
  size_t pbufferoutlen = 0;
  char hrmn[6];
  char *plastBS;
  int ret;

  ret = send(s, HTTP_VERSION, sizeof(HTTP_VERSION) - 1, 0);
  if (ret != sizeof(HTTP_VERSION) - 1)
    return -1;

  if (send(s, " ", 1, 0) != 1)
    return -1;

  ret = send(s, HTTP_CODE_STATUS_OK_STR, sizeof(HTTP_CODE_STATUS_OK_STR) - 1, 0);
  if (ret != sizeof(HTTP_CODE_STATUS_OK_STR) - 1)
    return -1;

  if (send(s, " ", 1, 0) != 1)
    return -1;

  ret = send(s, HTTP_STRING_STATUS_OK, sizeof(HTTP_STRING_STATUS_OK) - 1, 0);
  if (ret != sizeof(HTTP_STRING_STATUS_OK) - 1)
    return -1;

  ret = send(s, "\r\n", 2, 0);
  if (ret != 2)
    return -1;

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

    plastBS = strrchr(res->resource, '\\') + 1;
    if (strcmp(plastBS, "index") == 0 || strcmp(plastBS, "erreur_fichier_nul") == 0) {
      pbufferout = (char*)malloc(fsize + BufferUserNameSize + 6);
      ZeroMemory(pbufferout, fsize + BufferUserNameSize + 6);
      StringCchPrintfA(pbufferout, fsize + BufferUserNameSize + 6 - 1, pbufferin, BufferUserName, hrmn);
      free(pbufferin);
      pbufferoutlen = strlen(pbufferout);
    }
    else if (strcmp(plastBS, "succes") == 0) {
      pbufferout = (char*)malloc(fsize + 254 + 6);
      ZeroMemory(pbufferout, fsize + 254 + 6);
      StringCchPrintfA(pbufferout, fsize + 254 + 6,
        pbufferin,
        successinfo->filename,
        successinfo->filenameSize,
        successinfo->elapsedTime,
        successinfo->averagespeed,
        hrmn);
      free(pbufferin);
      pbufferoutlen = strlen(pbufferout);
    }
    else if (strcmp(plastBS, "credits") == 0 || strcmp(plastBS, "erreur_404") == 0 || strcmp(plastBS, "settings") == 0) {
      pbufferout = (char*)malloc(fsize + 6);
      ZeroMemory(pbufferout, fsize + 6);
      StringCchPrintfA(pbufferout, fsize + 6, pbufferin, hrmn);
      free(pbufferin);
      pbufferoutlen = strlen(pbufferout);
    }
    else {
      free(pbufferin);
      goto err;
    }

    ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
    create_http_header_nv(res, httpnv, pbufferoutlen);

    ret = send_http_header_nv(httpnv, s);
    if (ret > 0) {
      if (pbufferout)
        free(pbufferout);
      CloseHandle(hFile);
      return 8;
    }

    if (send(s, pbufferout, (int)pbufferoutlen, 0) <= 0) {
      free(pbufferout);
      CloseHandle(hFile);
      return 10;
    }

    ret = 0;
    free(pbufferout);
err:
    CloseHandle(hFile);
  } else if (strcmp(res->type, "image/png") == 0 || strcmp(res->type, "image/x-icon") == 0) {
    char buffer[1024];

    ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
    create_http_header_nv(res, httpnv, fsize);

    ret = send_http_header_nv(httpnv, s);
    if (ret > 0) {
      CloseHandle(hFile);
      return 11;
    }

    while (ReadFile(hFile, buffer, 1024, &read, NULL)) {
      send(s, buffer, read, 0);

      if (read < 1024)
          break;
    }

    CloseHandle(hFile);
    ret = 1;
  }

  return ret;
}
/*
errno_t
redir404(int s) {
  struct http_resource ewures;

  ZeroMemory(&ewures, sizeof(struct http_resource));
  memcpy_s(&ewures, sizeof(struct http_resource), &http_resources[16], sizeof(struct http_resource));
  http_serv_resource(&ewures, s, NULL);
  
  return 0;
}
*/
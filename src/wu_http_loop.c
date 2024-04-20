#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"
#include "wu_http_loop.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_nv.h"
#include "wu_content.h"


extern const struct _http_resources http_resources[];
extern struct wu_msg wumsg[];

int
http_match_resource(char *res)
{
  int i;

  if (res && *res == '/' && *(res + 1) == '\0') {
    return 0;
  } else if (res && *res && strcmp(res + 1, "favicon.ico") == 0) {
    return 1;
  }
  for (i = 2; http_resources[i].resource != NULL; i++) {
      if (strcmp(res + 1, http_resources[i].resource) == 0)
        return i;
  }

  return -1;
}

errno_t
create_local_resource(struct http_resource *lres, int ires, int theme) {
  unsigned char curDir[1024];
  int errn;

  ZeroMemory(curDir, 1024);
  if (!GetCurrentDirectoryA(1024, curDir)) {
    return 0;
  }

  if (strcmp(http_resources[ires].type, "text/html") == 0) {
    strcat_s(curDir, 1024, "\\html\\");
    if (!theme)
      strcat_s(curDir, 1024, "light\\");
    else
      strcat_s(curDir, 1024, "dark\\");

    strcat_s(curDir, 1024, http_resources[ires].resource);
  } else if (strcmp(http_resources[ires].type, "image/png") == 0) {
    strcat_s(curDir, 1024, "\\");
    strcat_s(curDir, 1024, http_resources[ires].resource);  
  } else if (strcmp(http_resources[ires].type, "x-icon") == 0) {
    strcat_s(curDir, 1024, "\\");
    strcat_s(curDir, 1024, http_resources[ires].resource);
  }

  strcpy_s(lres->type, HTTP_TYPE_MAX_LENGTH, http_resources[ires].type);
  strcpy_s(lres->resource, HTTP_RESSOURCE_MAX_LENGTH, curDir);

  _get_errno(&errn);

  return errn;
}

void
check_cookie_theme(struct header_nv hdrnv[], int *theme) {
  int res;

  res = nv_find_name_client(hdrnv, "Cookie");
  if (res < 0) {
    *theme = 0;
    return;
  } else if (strcmp(hdrnv[res].value.v, "theme=dark") == 0) {
    *theme = 1;
  } else if (strcmp(hdrnv[res].value.v, "theme=light") == 0) {
    *theme = 0;
  }

  return;
}


int
http_loop(HANDLE conScreenBuffer, COORD *cursorPosition, int s) {
  struct http_reqline reqline;
  struct header_nv httpnv[HEADER_NV_MAX_SIZE];
  unsigned char webuiquit = 0;
  struct http_resource httplocalres;
  int s_user;
  DWORD err;
  int theme;


webuiquit:
  s_user = accept_conn(conScreenBuffer, cursorPosition, s);
  
  ZeroMemory(&reqline, sizeof(struct http_reqline));
  if (http_recv_reqline(&reqline, s_user) != 0)
    goto err;

  ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
  if (http_recv_headernv(httpnv, s_user) != 0)
    goto err;

  if (strcmp(reqline.method, "GET") == 0) {
    int ires;
    struct http_resource httplocalres;    

    ires = http_match_resource(reqline.resource);
    if (ires < 0)
      goto err;

/*
     if (strcmp(reqline.resource + 1, "quit") == 0) {
      ires = 16;
      webuiquit = 8;
    } else if (strcmp(reqline.resource + 1, "openRep") == 0) {
      char dd[1024];

      create_download_directory(dd);
      ShellExecuteA(NULL, "open", dd, NULL, NULL, SW_SHOWNORMAL);
      if (strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/index") != 0)
        goto err;
      ires = 0;
    }   
*/
    check_cookie_theme(httpnv, &theme);

    ZeroMemory(&httplocalres, sizeof(struct http_resource));
    if (create_local_resource(&httplocalres, ires, theme) != 0) {
      cursorPosition->Y++;
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_GET_RESOURCE, NULL);
      Sleep(1000);
    }

    err = http_serv_resource(&httplocalres, s_user, NULL);
    if (err > 1) {
      cursorPosition->Y++;
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INFO(conScreenBuffer, ERR_FMT_MSG_CANNOT_SERV_RESOURCE, (void*)&err);
      Sleep(1000);
    } else if (err == 0) {
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INFO(conScreenBuffer, INF_MSG_INCOMING_CONNECTION, NULL);
      cursorPosition->Y++;
    } else if (strcmp(reqline.method, "POST") == 0 && strcmp(reqline.resource, "/") == 0) {
      struct user_stats upstats;

      clearTXRXPane(conScreenBuffer, cursorPosition);
      ZeroMemory(&upstats, sizeof(struct user_stats));
      err = receiveFile(conScreenBuffer, cursorPosition, httpnv, s_user, &upstats);
      cursorPosition->Y++;
    }
  }
  goto webuiquit;
err:
  closesocket(s_user);

  return 0;
}

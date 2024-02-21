#include <Windows.h>
#include <strsafe.h>

#include "ewu_msg.h"
#include "ewu_main.h"
#include "ewu_utils.h"
#include "ewu_socket.h"
#include "ewu_main_loop.h"
#include "ewu_http_receive.h"
#include "ewu_http.h"
#include "ewu_http_nv.h"


extern const struct http_resource http_resources[];
extern ewu_msg ewumsg[];


int
http_loop(HANDLE conScreenBuffer, COORD *cursorPosition, int s) {
  struct http_reqline reqline;
  struct header_nv httpnv[HEADER_NV_MAX_SIZE];
  unsigned char webuiquit = 0;
  int s_user;
  DWORD err;

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

    if (strcmp(reqline.resource + 1, "quit") == 0) {
      ires = 16;
      webuiquit = 8;
    }

    else if (strcmp(reqline.resource + 1, "openRep") == 0) {
      char dd[1024];

      create_download_directory(dd);
      ShellExecuteA(NULL, "open", dd, NULL, NULL, SW_SHOWNORMAL);
      if (strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/accueil") != 0)
        goto err;
      ires = 0;
    }


    ZeroMemory(&httplocalres, sizeof(struct http_resource));
    if (create_local_resource(&httplocalres, ires) != 0) {
      cursorPosition->Y++;
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INF(conScreenBuffer, ERR_MSG_CANNOT_GET_RESOURCE, NULL);
      Sleep(1000);
    }

    err = http_serv_resource(&httplocalres, s_user, NULL);
    if (err > 1) {
      cursorPosition->Y++;
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INF(conScreenBuffer, ERR_FMT_MSG_CANNOT_SERV_RESOURCE, (void*)&err);
      Sleep(1000);
    } else if (err == 0) {
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INF(conScreenBuffer, INF_MSG_INCOMING_CONNECTION, NULL);
      cursorPosition->Y++;
    }
    
    if (webuiquit > 0) {
      if (--webuiquit == 0) {
        Sleep(1666);
        return 1;
      }
      else {
        goto webuiquit;
      }
    }
  }
  else if (strcmp(reqline.method, "POST") == 0 && strcmp(reqline.resource, "/") == 0) {
    struct user_stats upstats;

    clearTXRXPane(conScreenBuffer, cursorPosition);
    ZeroMemory(&upstats, sizeof(struct user_stats));
    err = receiveFile(conScreenBuffer, cursorPosition, httpnv, s_user, &upstats);
    cursorPosition->Y++;
  }

err:
  closesocket(s_user);

  return 0;
}

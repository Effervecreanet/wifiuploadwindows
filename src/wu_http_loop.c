#include <Windows.h>
#include <strsafe.h>

#include "ewu_msg.h"
#include "ewu_main.h"
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


    // serv resource

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

#include <Windows.h>
#include <strsafe.h>
#include <time.h>

#include "wu_msg.h"
#include "wu_main.h"
#include "wu_socket.h"
#include "wu_http_loop.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_nv.h"
#include "wu_content.h"
#include "wu_http_theme.h"


extern const struct _http_resources http_resources[];
extern struct wu_msg wumsg[];
extern FILE *fp_log;


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

int
create_local_resource(struct http_resource *lres, int ires, int theme) {
  unsigned char curDir[1024];
  int errn;

  ZeroMemory(curDir, 1024);
  if (!GetCurrentDirectoryA(1024, curDir)) {
    return 1;
  }

  if (strcmp(http_resources[ires].type, "text/html") == 0) {
#ifdef VERSION_FR
    strcat_s(curDir, 1024, "\\html\\fr\\");
#else
    strcat_s(curDir, 1024, "\\html\\en\\");
#endif
    if (!theme)
      strcat_s(curDir, 1024, "light\\");
    else
      strcat_s(curDir, 1024, "dark\\");

    strcat_s(curDir, 1024, http_resources[ires].resource);
  } else if (strcmp(http_resources[ires].type, "image/png") == 0) {
    strcat_s(curDir, 1024, "\\");
    if (!theme)
      strcat_s(curDir, 1024, http_resources[ires].path_theme1);
    else
      strcat_s(curDir, 1024, http_resources[ires].path_theme2);  
  } else if (strcmp(http_resources[ires].type, "image/x-icon") == 0) {
    strcat_s(curDir, 1024, "\\");
    strcat_s(curDir, 1024, http_resources[ires].path_theme1);
  }


  strcpy_s(lres->type, HTTP_TYPE_MAX_LENGTH, http_resources[ires].type);
  strcpy_s(lres->resource, HTTP_RESSOURCE_MAX_LENGTH, curDir);

  return 0;
}

void
check_cookie_theme(struct header_nv hdrnv[], int *theme) {
  int res;

  res = nv_find_name_client(hdrnv, "Cookie");
  if (res < 0) {
    *theme = 0;
    return;
  } else if (strncmp(hdrnv[res].value.v, "theme=dark", sizeof("theme=dark") - 1) == 0) {
    *theme = 1;
  } else if (strncmp(hdrnv[res].value.v, "theme=light", sizeof("theme=light") - 1) == 0) {
    *theme = 0;
  }

  return;
}


int
http_loop(HANDLE conScreenBuffer, COORD *cursorPosition, int s, char logentry[256]) {
  struct http_reqline reqline;
  struct header_nv httpnv[HEADER_NV_MAX_SIZE];
  unsigned char webuiquit = 0;
  struct http_resource httplocalres;
  int s_user;
  DWORD err;
  int theme;
  char ipaddrstr[16];
  int errn;
	char log_timestr[42];
	struct tm *tmval;
	time_t wutime;
	int bytesent;

	bytesent = 0;
  memset(ipaddrstr, 0, 16);
  s_user = accept_conn(conScreenBuffer, cursorPosition, s, ipaddrstr);
  
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
    if (ires < 0) {
      check_cookie_theme(httpnv, &theme);

      for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

      ZeroMemory(&httplocalres, sizeof(struct http_resource));
      if (create_local_resource(&httplocalres, ires, theme) != 0) {
        cursorPosition->Y++;
        SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
        WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_GET_RESOURCE, NULL);
        Sleep(1000);
      }

      err = http_serv_resource(&httplocalres, s_user, NULL, &bytesent); 

      goto err;
      }

     if (strcmp(reqline.resource + 1, "quit") == 0) {
        WSACleanup();
		fclose(fp_log);
        ExitProcess(0);
    } else if (strcmp(reqline.resource + 1, "openRep") == 0) {
      char dd[1024];

      create_download_directory(dd);
      ShellExecuteA(NULL, "open", dd, NULL, NULL, SW_SHOWNORMAL);
      if (strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/index") != 0)
        goto err;
      ires = 0;
    }   

    check_cookie_theme(httpnv, &theme);

    ZeroMemory(&httplocalres, sizeof(struct http_resource));
    if (create_local_resource(&httplocalres, ires, theme) != 0) {
      cursorPosition->Y++;
      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_GET_RESOURCE, NULL);
      Sleep(1000);
    }

    err = http_serv_resource(&httplocalres, s_user, NULL, &bytesent);
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
      err = receiveFile(conScreenBuffer, cursorPosition, httpnv, s_user, &upstats, theme, &bytesent);
      cursorPosition->Y++;
    }
  } else if (strcmp(reqline.method, "POST") == 0) {
    if (strcmp(reqline.resource + 1, "theme") == 0) {
      char cookie[48];
      int theme;

      if (wu_http_recv_theme(httpnv, s_user, &theme) < 0)
        goto err;

      ZeroMemory(cookie, 48);
      if (theme == 0)
        strcpy_s(cookie, 48, "theme=dark");
      else
        strcpy_s(cookie, 48, "theme=light");

      if (apply_theme(s_user, cookie) < 0)
          return -1;


      /* Use ret here if you want */
    } else if (strcmp(reqline.resource + 1, "upload") == 0) {
        struct user_stats upstats;

        clearTXRXPane(conScreenBuffer, cursorPosition);
        ZeroMemory(&upstats, sizeof(struct user_stats));
        err = receiveFile(conScreenBuffer, cursorPosition, httpnv, s_user, &upstats, theme, &bytesent);
        cursorPosition->Y++;      
    }
  }

	ZeroMemory(logentry, 256);
	ZeroMemory(log_timestr, 42);

	time(&wutime);
	tmval = localtime(&wutime);

	strftime(log_timestr, 42, "%d/%b/%Y:%T -0600", tmval);
	sprintf(logentry, "%s - - [%s] \"%s %s %s\" 200 %i\n", ipaddrstr, log_timestr,
														   reqline.method, reqline.resource,
														   reqline.version, bytesent);

err:
  closesocket(s_user);

  return 0;
}


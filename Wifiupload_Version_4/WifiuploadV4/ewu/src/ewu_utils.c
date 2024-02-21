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

errno_t
create_local_resource(struct http_resource *lres, int ires) {
  unsigned char curDir[1024];
  int errn;

  ZeroMemory(curDir, 1024);
  if (!GetCurrentDirectoryA(1024, curDir)) {
    return 0;
  }

  if (ires == 0 || ires == 1 || (ires >= 12 && ires <= 16)) {
    strcat_s(curDir, 1024, "\\html\\");
    strcat_s(curDir, 1024, http_resources[ires].resource);
  }
  else if (ires == 2) {
    strcat_s(curDir, 1024, "\\");
    strcat_s(curDir, 1024, http_resources[ires].resource);
  }
  else {
    strcat_s(curDir, 1024, "\\images\\");
    strcat_s(curDir, 1024, strchr(http_resources[ires].resource, '/') + 1);
  }

  strcpy_s(lres->type, HTTP_TYPE_MAX_LENGTH, http_resources[ires].type);
  strcpy_s(lres->resource, HTTP_RESSOURCE_MAX_LENGTH, curDir);

  _get_errno(&errn);

  return errn;
}

int
create_download_directory(unsigned char dd[1024]) {
    DWORD szdd = 1024;

    ZeroMemory(dd, 1024);
    GetUserProfileDirectoryA(GetCurrentProcessToken(), dd, &szdd);
    strcat_s(dd, 1024, "\\Downloads\\");

    return 1;
}

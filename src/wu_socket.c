#include <Windows.h>
#include <strsafe.h>

#include "wu_msg.h"

#define LISTEN_PORT 80

int *listensocket;

int
create_socket(HANDLE conScreenBuffer, COORD *cursorPosition) {
	int s;
	INPUT_RECORD inRec;
	DWORD read;

  s = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  if (s == INVALID_SOCKET) {
    CHAR Buffer[1024];
    DWORD written;

    ZeroMemory(Buffer, 1024);
    
    StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_1, WSAGetLastError());
    cursorPosition->Y++;
    
    SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
    WriteConsoleA(conScreenBuffer, Buffer, (DWORD)strlen(Buffer), &written, NULL);

	while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

  return s;
}

void
bind_socket(HANDLE conScreenBuffer, COORD* cursorPosition, int s, struct in_addr inaddr) {
  struct sockaddr_in sainServer;
  char tval = 1;

  ZeroMemory(&sainServer, sizeof(struct sockaddr_in));
  sainServer.sin_addr = inaddr;
  sainServer.sin_family = AF_INET;
  sainServer.sin_port = htons(LISTEN_PORT);

  if (0 != bind(s, (const struct sockaddr*)&sainServer, sizeof(struct sockaddr_in))) {
    CHAR Buffer[1024];
    DWORD written;
	INPUT_RECORD inRec;
	DWORD read;

    ZeroMemory(Buffer, 1024);

    StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_2, WSAGetLastError());
    cursorPosition->Y++;

    SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
    WriteConsoleA(conScreenBuffer, Buffer, (DWORD)strlen(Buffer), &written, NULL);

	while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
  }

  setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&tval, sizeof(char));

  listen(s, 10);

  listensocket = &s;

  return;
}

int
accept_conn(HANDLE conScreenBuffer, COORD* cursorPosition, int s, char ipaddrstr[16]) {
  int s_user, sainLen;
  struct sockaddr_in sainUser;

  ZeroMemory(&sainUser, sizeof(struct sockaddr_in));  
  sainLen = sizeof(struct sockaddr);
  
  for (;;) {
    s_user = (int)accept(s, (struct sockaddr*)&sainUser, &sainLen);
    
    if (s_user == INVALID_SOCKET) {
      CHAR Buffer[1024];
      DWORD written;

      ZeroMemory(Buffer, 1024);

      StringCchPrintf(Buffer, 1024, ERROR_MESSAGE_SOCKET_3, WSAGetLastError());
      cursorPosition->Y++;

      SetConsoleCursorPosition(conScreenBuffer, *cursorPosition);
      WriteConsoleA(conScreenBuffer, Buffer, (DWORD)strlen(Buffer), &written, NULL);

      Sleep(100);
    }
    else {
	    char *ipstr;
	    ipstr = inet_ntoa(sainUser.sin_addr);
	    memcpy(ipaddrstr, ipstr, strlen(ipstr));
      break;
    }
  }


  return s_user;
}




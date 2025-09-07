#include <Windows.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_tls_https.h"
#include "wu_http_nv.h"

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE *g_fphttpslog;

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

int
https_serv_resource(struct http_resource *res, int s,
                   struct success_info *successinfo,
				   int *bytesent, CtxtHandle *ctxtHandle) {
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
	SecPkgContext_StreamSizes Sizes;
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[4];
	int i;
	char *encryptBuffer;
	char *message;
	int encryptBufferLen;
	int messageLen;

	QueryContextAttributes(ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &Sizes);

	ZeroMemory(&bufferDesc, sizeof(SecBufferDesc));
	bufferDesc.ulVersion = SECBUFFER_VERSION;
	bufferDesc.cBuffers = 4;
	bufferDesc.pBuffers = secBufferOut;

	encryptBufferLen = Sizes.cbHeader + Sizes.cbMaximumMessage + Sizes.cbTrailer;
	encryptBuffer = malloc(encryptBufferLen);
	ZeroMemory(encryptBuffer, encryptBufferLen);
	message = encryptBuffer + Sizes.cbHeader;
	sprintf_s(message, encryptBufferLen - Sizes.cbHeader, "%s %s %s\r\n", HTTP_VERSION, HTTP_CODE_STATUS_OK_STR, HTTP_STRING_STATUS_OK);
	messageLen = strlen(message);
	ZeroMemory(secBufferOut, sizeof(SecBuffer) * 4);
	secBufferOut[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBufferOut[0].pvBuffer = encryptBuffer;
	secBufferOut[0].cbBuffer = Sizes.cbHeader;
	secBufferOut[1].BufferType = SECBUFFER_DATA;
	secBufferOut[1].pvBuffer = message;
	secBufferOut[1].cbBuffer = messageLen;
	secBufferOut[2].BufferType = SECBUFFER_STREAM_TRAILER;
	secBufferOut[2].pvBuffer = message + messageLen;
	secBufferOut[2].cbBuffer = Sizes.cbTrailer;
	secBufferOut[3].BufferType = SECBUFFER_EMPTY;

	ret = EncryptMessage(ctxtHandle, 0, &bufferDesc, 0);
	fprintf(g_fphttpslog, "EncryptMessage: %x\n", ret);
	fflush(g_fphttpslog);

	for(i = 0; i < 4; i++)
		if (secBufferOut[i].BufferType == SECBUFFER_DATA)
			break;

	if (send(s, secBufferOut[i].pvBuffer, secBufferOut[i].cbBuffer, 0) < 0)
		return -1;

	*bytesent += strlen(messageLen);
/*
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
    else if (strcmp(plastBS, "success") == 0) {
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

    ret = send_http_header_nv(httpnv, s, bytesent);
    if (ret > 0) {
      if (pbufferout)
        free(pbufferout);
      CloseHandle(hFile);
      return 8;
    }

	ret = send(s, pbufferout, (int)pbufferoutlen, 0);
	if (ret <= 0) {
      free(pbufferout);
      CloseHandle(hFile);
      return 10;
    }

	*bytesent += ret;

    ret = 0;
    free(pbufferout);
err:
    CloseHandle(hFile);
  } else if (strcmp(res->type, "image/png") == 0 || strcmp(res->type, "image/x-icon") == 0) {
    char buffer[1024];

    ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
    create_http_header_nv(res, httpnv, fsize);

    ret = send_http_header_nv(httpnv, s, bytesent);
    if (ret > 0) {
      CloseHandle(hFile);
      return 11;
    }

	*bytesent += ret;

    while (ReadFile(hFile, buffer, 1024, &read, NULL)) {
      ret = send(s, buffer, read, 0);

		*bytesent += ret;

      if (read < 1024)
          break;
    }

    CloseHandle(hFile);
    ret = 1;
  }
*/
  return ret;
}


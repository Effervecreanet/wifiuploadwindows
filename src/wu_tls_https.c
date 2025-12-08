#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_tls_https.h"
#include "wu_http_nv.h"

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE* g_fphttpslog;
extern int g_tls_firstsend;


int get_request_line(struct http_reqline* reqline, char* BufferIn, int length)
{
	char* p_bufferin = BufferIn;
	int i = 0, count = 0;

	if (length == 0)
		return -1;

	do {
		reqline->method[i++] = *p_bufferin++;
		if (*p_bufferin == ' ')
			break;
	} while (i <= REQUEST_LINE_METHOD_MAX_SIZE);

	if (*p_bufferin != ' ')
		return -1;
	p_bufferin++;

	count += i;
	i = 0;

	do {
		reqline->resource[i++] = *p_bufferin++;
		if (*p_bufferin == ' ')
			break;
	} while (i <= HTTP_RESSOURCE_MAX_LENGTH);

	if (*p_bufferin != ' ')
		return -1;
	p_bufferin++;

	count += i;
	i = 0;

	do {
		reqline->version[i++] = *p_bufferin++;
		if (*p_bufferin == '\r')
			break;
	} while (i <= sizeof("HTTP/1.1") - 1);

	if (*p_bufferin != '\r')
		return -1;

	count += i;
	count += 4; /* Add ignored characters count and skip "\r\n" */

	return count;
}

int get_header_nv(struct header_nv headernv[HEADER_NV_MAX_SIZE], char* buffer, int bufferlength)
{
	int count_hdr_nv = 0;
	int headerlen = 0;
	int i;

	if (bufferlength == 0)
		return -1;

	do {

		i = 0;

		do {
			if (isalpha(*buffer) != 0 || *buffer == '-' || *buffer == '.')
				headernv[count_hdr_nv].name.client[i++] = *buffer++;
			else if (*buffer == ':' && *(buffer + 1) == ' ')
				break;
			else if (*buffer == '\r' && *(buffer + 1) == '\n')
				goto crlfcrlf;
			if (headerlen++ >= HEADER_MAX_SIZE)
				return -1;
		} while (i < HEADER_NAME_MAX_SIZE);

		if (i == HEADER_NAME_MAX_SIZE)
			return -1;

		buffer += 2;
		headerlen += 2;

		if (headerlen >= HEADER_MAX_SIZE)
			return -1;

		i = 0;

		do {
			headernv[count_hdr_nv].value.v[i++] = *buffer++;
			if (*buffer == '\r' && *(buffer + 1) == '\n')
				break;
			if (headerlen++ >= HEADER_MAX_SIZE)
				return -1;
		} while (i < HEADER_VALUE_MAX_SIZE);

		if (i == HEADER_VALUE_MAX_SIZE)
			return -1;

		buffer += 2;
		headerlen += 3;

		if (headerlen >= HEADER_MAX_SIZE)
			return -1;
	} while (++count_hdr_nv < HEADER_NV_MAX_SIZE);

crlfcrlf:
	buffer += 2;
	headerlen += 2;

	if (headerlen >= HEADER_MAX_SIZE)
		return -1;

	return headerlen;
}

int
https_serv_resource(struct http_resource* res, int s,
	struct success_info* successinfo,
	int* bytesent, CtxtHandle* ctxtHandle,
	COORD cursorPosition) {
	HANDLE hFile;
	DWORD fsize, read, err;
	DWORD BufferUserNameSize = 254;
	SYSTEMTIME sysTime;
	struct header_nv httpnv[HEADER_NV_MAX_SIZE];
	char* pbufferin = NULL, * pbufferout = NULL;
	char BufferUserName[254];
	size_t pbufferoutlen = 0;
	char hrmn[6];
	char* plastBS;
	int ret;
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[4];
	int i;
	char message[8192];
	int encryptBufferLen;
	int messageLen;

	ZeroMemory(message, 8192);
	sprintf_s(message, 8192, "%s %s %s\r\n", HTTP_VERSION, HTTP_CODE_STATUS_OK_STR, HTTP_STRING_STATUS_OK);

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
		create_http_header_nv(res, httpnv, pbufferoutlen, -1);

		for (i = 0; i < HEADER_NV_MAX_SIZE && httpnv[i].name.wsite != NULL; i++) {
			messageLen = strlen(message);
			sprintf_s(message + messageLen, 8192 - messageLen,
				"%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, 8192 - messageLen, "\r\n");
		strcat_s(message, 8192 - messageLen - 2, pbufferout);

		fprintf(g_fphttpslog, "message:\n%s\n", message);
		fflush(g_fphttpslog);

		tls_send(s, ctxtHandle, message, strlen(message), cursorPosition);

		*bytesent += messageLen;

		ret = 0;
		free(pbufferout);
	err:
		CloseHandle(hFile);
	}
	else if (strcmp(res->type, "image/png") == 0 || strcmp(res->type, "image/x-icon") == 0) {
		char buffer[1024];

		ZeroMemory(httpnv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		create_http_header_nv(res, httpnv, fsize, -1);

		for (i = 0; i < HEADER_NV_MAX_SIZE && httpnv[i].name.wsite != NULL; i++) {
			messageLen = strlen(message);
			sprintf_s(message + messageLen, 8192 - messageLen, "%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, 8192 - messageLen, "\r\n");

		messageLen = strlen(message);

		*bytesent += strlen(message);

		tls_send(s, ctxtHandle, message, strlen(message), cursorPosition);

		while (ReadFile(hFile, message, 2048, &read, NULL) != 0 && read) {
			tls_send(s, ctxtHandle, message, read, cursorPosition);

			*bytesent += read;
		}

		CloseHandle(hFile);
		ret = 1;
	}

	return ret;
}


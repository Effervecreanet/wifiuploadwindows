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
	char* p, * psav, * sp;
	int i;

	for (i = 0, psav = p = BufferIn; i < length - 2; p++, i++)
		if (*p == '\r' && *(p + 1) == '\n')
			break;

	if (i >= length - 2)
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

int get_header_nv(struct header_nv headernv[HEADER_NV_MAX_SIZE], char* buffer)
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

	return count2; 
}

int
https_serv_resource(struct http_resource* res, int s,
	struct success_info* successinfo,
	int* bytesent, CtxtHandle* ctxtHandle) {
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
	static SecPkgContext_StreamSizes Sizes;
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[4];
	int i;
	char* encryptBuffer;
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
			sprintf_s(message + messageLen, Sizes.cbMaximumMessage - messageLen,
				"%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, Sizes.cbMaximumMessage - messageLen, "\r\n");
		strcat_s(message, Sizes.cbMaximumMessage - messageLen, pbufferout);

		messageLen = strlen(message);
		tls_send(s, ctxtHandle, message, strlen(message));

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
			sprintf_s(message + messageLen, Sizes.cbMaximumMessage - messageLen, "%s: %s\r\n", httpnv[i].name.wsite,
				httpnv[i].value.pv == NULL ? httpnv[i].value.v : httpnv[i].value.pv);
		}

		messageLen = strlen(message);
		strcat_s(message, Sizes.cbMaximumMessage - messageLen, "\r\n");

		messageLen = strlen(message);

		*bytesent += strlen(message);

		tls_send(s, ctxtHandle, message, strlen(message));

		while (ReadFile(hFile, message, 2048, &read, NULL) != 0 && read) {
			tls_send(s, ctxtHandle, message, read);

			*bytesent += read;
		}

		CloseHandle(hFile);
		ret = 1;
	}

	return ret;
}


#include <Windows.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_nv.h"

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE *g_fphttpslog;

void
wu_https_post_theme_hdr_nv(struct header_nv hdrnv[32], char* cookie)
{
	int cnt = 0;
	time_t now = time(NULL);
	struct tm tmv;
	char expires[HTTP_STRING_DATE_SIZE];

	hdrnv[cnt].name.wsite = HTTP_HEADER_CACHE_CONTROL;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CACHE_CONTROL_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONNECTION;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CONNECTION_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_SERVER;
	hdrnv[cnt++].value.pv = HTTP_HEADER_SERVER_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONTENT_LENGTH;
	strcpy_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE, "0");

	hdrnv[cnt].name.wsite = HTTP_HEADER_CONTENT_LANGUAGE;
	hdrnv[cnt++].value.pv = HTTP_HEADER_CONTENT_LANGUAGE_VALUE;

	hdrnv[cnt].name.wsite = HTTP_HEADER_SET_COOKIE;

	ZeroMemory(&tmv, sizeof(struct tm));
	gmtime_s(&tmv, &now);
	tmv.tm_year++;
	strftime(expires, HTTP_STRING_DATE_SIZE, "%a, %d %b %Y %H:%M:%S GMT", &tmv);

	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, cookie);
	strcat_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, "; Expires=");
	strcat_s(hdrnv[cnt++].value.v, HEADER_VALUE_MAX_SIZE, expires);

	hdrnv[cnt].name.wsite = HTTP_HEADER_LOCATION;
	strcpy_s(hdrnv[cnt].value.v, HEADER_VALUE_MAX_SIZE, "/");

	return;
}


int
https_apply_theme(int s_clt, CtxtHandle *ctxtHandle, char* cookie)
{
	struct header_nv hdrnv[32];
	char buffer[2048];
	char *encryptbuffer, *message;
	int i, encryptbuffer_len, message_len;
	SecPkgContext_StreamSizes Sizes;
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[4];

	QueryContextAttributes(ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &Sizes);
	encryptbuffer_len = Sizes.cbHeader + Sizes.cbMaximumMessage + Sizes.cbTrailer;

	encryptbuffer = malloc(encryptbuffer_len);
	ZeroMemory(encryptbuffer, encryptbuffer_len);

	message = encryptbuffer + Sizes.cbHeader;

	memset(hdrnv, 0, sizeof(struct header_nv) * 32);
	wu_https_post_theme_hdr_nv(hdrnv, cookie);

	ZeroMemory(buffer, 2048);
	strcpy_s(buffer, 2048, "HTTP/1.1 301 Moved Permanently\r\n");

	for (i = 0; i < 32; i++) {
		if (hdrnv[i].name.wsite == NULL)
			break;

		strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].name.wsite);
		strcat_s(buffer, 2048 - strlen(buffer), ": ");

		if (hdrnv[i].value.pv != NULL)
			strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].value.pv);
		else
			strcat_s(buffer, 2048 - strlen(buffer), hdrnv[i].value.v);

		strcat_s(buffer, 2048 - strlen(buffer), "\r\n");

	}

	strcat_s(buffer, 2048 - strlen(buffer), "\r\n");

	strcpy_s(message, Sizes.cbMaximumMessage, buffer);
	message_len = strlen(message);

	ZeroMemory(&bufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&secBufferOut, sizeof(SecBuffer) * 4);
	bufferDesc.ulVersion = SECBUFFER_VERSION;
	bufferDesc.cBuffers = 4;
	bufferDesc.pBuffers = secBufferOut;

	secBufferOut[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBufferOut[0].pvBuffer = encryptbuffer;
	secBufferOut[0].cbBuffer = Sizes.cbHeader;
	secBufferOut[1].BufferType = SECBUFFER_DATA;
	secBufferOut[1].pvBuffer = message;
	secBufferOut[1].cbBuffer = message_len;
	secBufferOut[2].BufferType = SECBUFFER_STREAM_TRAILER;
	secBufferOut[2].pvBuffer = message + message_len;
	secBufferOut[2].cbBuffer = Sizes.cbTrailer;
	secBufferOut[3].BufferType = SECBUFFER_EMPTY;
	
	EncryptMessage(ctxtHandle, 0, &bufferDesc, 0);

	for (i = 0; i < 4; i++)
		if (secBufferOut[i].BufferType == SECBUFFER_DATA)
			break;

	if (send(s_clt, encryptbuffer, Sizes.cbHeader + message_len + Sizes.cbTrailer, 0) < 0)
		return -1;

	return 1;
}

int
get_theme_param(struct header_nv *headernv, char *bufferIn, int *theme) {
	int idxclen;
	int clen;
	int res;

	idxclen = nv_find_name_client(headernv, "Content-Length");
	if (idxclen < 0) 
		return -1;

	clen = atoi((headernv + idxclen)->value.v);

	if (strncmp(bufferIn, "theme=light", clen) == 0)
		*theme = 1;
	else if (strncmp(bufferIn, "theme=dark", clen) == 0)
			*theme = 0;
	else
		return -1;

	return 0;
}

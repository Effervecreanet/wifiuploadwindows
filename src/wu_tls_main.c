#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_main.h"
#include "wu_msg.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_nv.h"
#include "wu_content.h"
#include "wu_tls_main.h"
#include "wu_tls_x509.h"
#include "wu_tls_conn.h"
#include "wu_tls_https.h"

extern FILE* g_fphttpslog;
extern const struct _http_resources http_resources[];
extern HANDLE g_hConsoleOutput;
extern int g_tls_firstsend;
extern CredHandle* g_credHandle;
extern CtxtHandle* g_ctxtHandle;
extern int* g_tls_sclt;

int
handle_get_request(int s_clt, COORD cursorPosition, CtxtHandle ctxtHandle, char* resource, struct header_nv* headernv) {
	int ires;
	struct http_resource httplocalres;
	int theme, bytesent;

	ires = http_match_resource(resource);
	if (ires < 0) {
		check_cookie_theme(headernv, &theme);

		for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

		ZeroMemory(&httplocalres, sizeof(struct http_resource));
		if (create_local_resource(&httplocalres, ires, theme) != 0) {
			INPUT_RECORD inRec;
			DWORD read;

			cursorPosition.Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition);
			write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}

		https_serv_resource(&httplocalres, s_clt, NULL, &bytesent, &ctxtHandle);
	}
	else {
		check_cookie_theme(headernv, &theme);

		ZeroMemory(&httplocalres, sizeof(struct http_resource));
		if (create_local_resource(&httplocalres, ires, theme) != 0) {
			INPUT_RECORD inRec;
			DWORD read;

			cursorPosition.Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition);
			write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}

		https_serv_resource(&httplocalres, s_clt, NULL, &bytesent, &ctxtHandle);
	}

	return bytesent;
}

int
handle_theme_request(int s_clt, CtxtHandle ctxtHandle, CredHandle credHandle, struct header_nv* headernv, char* buffer) {
	char cookie[48];
	int err;
	int theme;

	err = get_theme_param(headernv, buffer, &theme);
	if (err != 0) {
		tls_shutdown(&ctxtHandle, &credHandle, s_clt);
		return -1;
	}

	ZeroMemory(cookie, 48);
	if (theme == 0)
		strcpy_s(cookie, 48, "theme=dark");
	else
		strcpy_s(cookie, 48, "theme=light");

	https_apply_theme(s_clt, &ctxtHandle, cookie);

	return 0;
}

int
handle_upload_request(int s_clt, COORD cursorPosition, struct header_nv* headernv, CtxtHandle ctxtHandle, char *buffer) {
	struct user_stats upstats;
	int theme;
	int bytesent;

	clear_txrx_pane(&cursorPosition);
	check_cookie_theme(headernv, &theme);

	ZeroMemory(&upstats, sizeof(struct user_stats));
	tls_receive_file(cursorPosition, headernv, s_clt, &upstats, theme, &bytesent, &ctxtHandle, buffer);
	cursorPosition.Y++;

	return bytesent;
}

DWORD WINAPI wu_tls_loop(struct paramThread* prThread)
{
	BYTE pbEncodedName[128];
	WSADATA wsaData;
	DWORD err, ret;
	int s, s_clt;
	CERT_CONTEXT* pCertContext;
	CredHandle credHandle;
	CtxtHandle ctxtHandle;
	HCERTSTORE hCertStore;
	char BufferIn[2000];
	int i;
	struct http_reqline reqline;
	INPUT_RECORD inRec;
	DWORD read;
	struct header_nv headernv[HEADER_NV_MAX_SIZE];
	int bytesent = 0;
	int theme = 0;
	int bytereceived = 0;
	int data_idx;
	SecBuffer secBufferIn[4];
	SecPkgContext_StreamSizes Sizes;
	char https_logentry[256];
	char ipaddr_httpsclt[16];
	char log_timestr[42];
	time_t wutime;
	struct tm tmval;
	NCRYPT_KEY_HANDLE hKey;
	NCRYPT_PROV_HANDLE phProvider;


	pCertContext = (CERT_CONTEXT*)find_mycert_in_store(&hCertStore);
	if (pCertContext) {
		if (is_certificate_valid(pCertContext) != 0)
			goto createcert;
	}
	else {
	createcert:
		create_certificate(prThread->cursorPosition, hCertStore, &pCertContext, pbEncodedName, &phProvider, &hKey, prThread->inaddr);
	}

	CertCloseStore(hCertStore, 0);

	ZeroMemory(&credHandle, sizeof(CredHandle));
	if (get_credantials_handle(&credHandle, pCertContext) < 0) {
		CertFreeCertificateContext(pCertContext); LocalFree(pbEncodedName); NCryptFreeObject(phProvider); NCryptFreeObject(hKey);
		err = GetLastError();
		write_info_in_console(ERR_MSG_ACQUIRECREDANTIALSHANDLE, NULL, err);
		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}


	s = create_socket(&prThread->cursorPosition);
	bind_socket2(&prThread->cursorPosition, s, prThread->inaddr);

	ZeroMemory(headernv, HEADER_NV_MAX_SIZE * (HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE));

	for (;;) {

		g_credHandle = g_ctxtHandle = NULL;
		g_tls_sclt = NULL;

		ZeroMemory(ipaddr_httpsclt, 16);

		s_clt = acceptSecure(s, &credHandle, &ctxtHandle, ipaddr_httpsclt);

		g_credHandle = &credHandle;
		g_ctxtHandle = &ctxtHandle;
		g_tls_sclt = &s_clt;

	next_req:
		bytesent = 0;

		ZeroMemory(secBufferIn, sizeof(SecBuffer) * 4);
		secBufferIn[0].pvBuffer = BufferIn;
		secBufferIn[0].BufferType = SECBUFFER_DATA;
		secBufferIn[0].cbBuffer = 2000;

		ZeroMemory(BufferIn, 2000);

		ret = tls_recv(s_clt, &ctxtHandle, secBufferIn, &bytereceived, &data_idx);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}

		ZeroMemory(&reqline, sizeof(struct http_reqline));
		ret = get_request_line(&reqline, secBufferIn[data_idx].pvBuffer, secBufferIn[data_idx].cbBuffer);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}

		if (strcmp(reqline.version, HTTP_VERSION) != 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}

		ZeroMemory(headernv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);

		err = get_header_nv(headernv, (char*)secBufferIn[data_idx].pvBuffer + ret);
		if (err < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}
		ret += err;

		i = nv_find_name_client(headernv, "Host");
		if (i < 0 || strcmp(headernv[i].value.v, inet_ntoa(prThread->inaddr)) != 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}

		if (strcmp(reqline.method, "GET") == 0) {
			bytesent = handle_get_request(s_clt, prThread->cursorPosition, ctxtHandle, reqline.resource, headernv);
		}
		else if (strcmp(reqline.method, "POST") == 0) {
			if (strcmp(reqline.resource, "/theme") == 0) {
				if (handle_theme_request(s_clt, ctxtHandle, credHandle, headernv,
					(char*)secBufferIn[data_idx].pvBuffer + ret + 2) < 0)
					continue;
			}
			else if (strcmp(reqline.resource, "/upload") == 0) {
				bytesent = handle_upload_request(s_clt, prThread->cursorPosition, headernv, ctxtHandle,
					(char*)secBufferIn[data_idx].pvBuffer + ret + 2);
			}
		}
		ZeroMemory(https_logentry, 256);
		ZeroMemory(log_timestr, 42);

		time(&wutime);

		ZeroMemory(&tmval, sizeof(struct tm));
		localtime_s(&tmval, &wutime);

		strftime(log_timestr, 42, "%d/%b/%Y:%T -600", &tmval);
		sprintf_s(https_logentry, 256, "%s - - [%s] \"%s %s %s\" 200 %i\n", ipaddr_httpsclt, log_timestr, reqline.method, reqline.resource, reqline.version, bytesent);
		fprintf(g_fphttpslog, https_logentry);
		fflush(g_fphttpslog);

		goto next_req;
	}

	CertFreeCertificateContext(pCertContext);
	LocalFree(pbEncodedName);
	NCryptFreeObject(phProvider);
	NCryptFreeObject(hKey);

	WSACleanup();


	return 0;
}


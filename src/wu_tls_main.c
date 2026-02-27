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
extern FILE* g_fplog;
extern int* g_usersocket;
extern int* g_listensocket;
extern HANDLE g_hNewFile_tmp;
extern char* g_sNewFile_tmp;
extern const struct _http_resources http_resources[];
extern HANDLE g_hConsoleOutput;
extern int g_tls_firstsend;
extern CredHandle* g_credHandle;
extern CtxtHandle* g_ctxtHandle;
extern int* g_tls_sclt;

SecPkgContext_StreamSizes context_sizes;
char* encryptBuffer = NULL;

void
https_wu_quit_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, int s_user, int* bytesent) {
	int ires;
	struct http_resource httplocalres;

	check_cookie_theme(httpnv, theme);

	for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

	ZeroMemory(&httplocalres, sizeof(struct http_resource));
	if (create_local_resource(&httplocalres, ires, *theme) != 0)
		show_error_wait_close(&cursorPosition[0], ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

	https_serv_resource(&httplocalres, s_user, NULL, bytesent, g_ctxtHandle, *cursorPosition);

	return;
}

void
https_quit_wu(int s_clt) {
	int i;

	if (g_fplog != NULL)
		fclose(g_fplog);
	if (g_fphttpslog != NULL)
		fclose(g_fphttpslog);
	if (g_usersocket != NULL && *g_usersocket != 0)
		closesocket(*g_usersocket);
	if (g_listensocket != NULL)
		closesocket(*g_listensocket);
	if (g_hConsoleOutput != INVALID_HANDLE_VALUE)
		CloseHandle(g_hConsoleOutput);
	if (g_hNewFile_tmp != INVALID_HANDLE_VALUE) {
		CloseHandle(g_hNewFile_tmp);
		DeleteFileA((LPCSTR)g_sNewFile_tmp);
	}
	if (g_credHandle != NULL && g_ctxtHandle != NULL &&
		g_tls_sclt != NULL)
		tls_shutdown(g_credHandle, g_ctxtHandle, *g_tls_sclt);
	if (encryptBuffer != NULL)
		free(encryptBuffer);
	WSACleanup();
	ExitProcess(TRUE);

	return;
}

void
accept_sec_conn(CtxtHandle *ctxtHandle, CredHandle *credHandle, int s, int *s_clt,
				char *ipaddr_httpsclt, COORD cursorPosition[2]) {
	*s_clt = acceptSecure(s, credHandle, ctxtHandle, ipaddr_httpsclt);
	if (cursorPosition[0].Y > cursorPosition[1].Y + 5) {
		cursorPosition[0] = cursorPosition[1];
		clear_txrx_pane(&cursorPosition[0]);
	}

	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);
	write_info_in_console(INF_MSG_INCOMING_CONNECTION, NULL, 0);
	cursorPosition[0].Y++;
	SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition[0]);

	g_credHandle = credHandle;
	g_ctxtHandle = ctxtHandle;
	g_tls_sclt = s_clt;

	if (encryptBuffer != NULL)
		free(encryptBuffer);

	QueryContextAttributes(ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &context_sizes);
	encryptBuffer = malloc(context_sizes.cbHeader + context_sizes.cbMaximumMessage + context_sizes.cbTrailer);

	return;
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
	int i;
	struct http_reqline reqline;
	INPUT_RECORD inRec;
	DWORD read;
	struct header_nv headernv[HEADER_NV_MAX_SIZE];
	int bytesent = 0;
	int theme = 0;
	char https_logentry[256];
	char ipaddr_httpsclt[16];
	char log_timestr[42];
	time_t wutime;
	struct tm tmval;
	NCRYPT_KEY_HANDLE hKey;
	NCRYPT_PROV_HANDLE phProvider;
	int header_offset;
	char* tls_recv_output;
	unsigned int tls_recv_output_size;

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
		CertFreeCertificateContext(pCertContext); NCryptFreeObject(phProvider); NCryptFreeObject(hKey);
		err = GetLastError();
		show_error_wait_close(&prThread->cursorPosition[0], ERR_MSG_ACQUIRECREDANTIALSHANDLE, NULL, err);
	}


	s = create_socket(&prThread->cursorPosition);
	bind_socket2(&prThread->cursorPosition, s, prThread->inaddr);

	ZeroMemory(headernv, HEADER_NV_MAX_SIZE * (HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE));

	for (;;) {

		g_credHandle = g_ctxtHandle = NULL;
		g_tls_sclt = NULL;

		ZeroMemory(ipaddr_httpsclt, 16);
		accept_sec_conn(&ctxtHandle, &credHandle, s, &s_clt, ipaddr_httpsclt,  prThread->cursorPosition);

	next_req:
		bytesent = 0;
		tls_recv_output = NULL;

		ret = get_https_request(&ctxtHandle, s_clt, &tls_recv_output, &tls_recv_output_size, &prThread->cursorPosition[0],
			&reqline, headernv, prThread->inaddr);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			if (tls_recv_output != NULL)
				free(tls_recv_output);
			continue;
		}

		header_offset = ret;

		if (strcmp(reqline.method, "GET") == 0) {
			ret = handle_get_request(&ctxtHandle, s_clt, &reqline, headernv, &bytesent, &prThread->cursorPosition[0]);
			if (ret < 0) {
				tls_shutdown(&ctxtHandle, &credHandle, s_clt);
				free(tls_recv_output);
				continue;
			}
		}
		else if (strcmp(reqline.method, "POST") == 0) {
			ret = handle_post_request(&ctxtHandle, s_clt, &reqline, headernv, &bytesent, tls_recv_output + header_offset,
				&prThread->cursorPosition);
			if (ret < 0) {
				tls_shutdown(&ctxtHandle, &credHandle, s_clt);
				free(tls_recv_output);
				continue;
			}
		}

		log_https_request(ipaddr_httpsclt, &reqline, bytesent);
		free(tls_recv_output);

		goto next_req;
	}

	CertFreeCertificateContext(pCertContext);
	NCryptFreeObject(phProvider);
	NCryptFreeObject(hKey);

	WSACleanup();


	return 0;
}

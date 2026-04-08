#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#define SCHANNEL_USE_BLACKLIST

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_msg.h"
#include "wu_main.h"
#include "wu_txstats.h"
#include "wu_http_nv.h"
#include "wu_http_receive.h"
#include "wu_http.h"
#include "wu_http_loop.h"
#include "wu_content.h"
#include "wu_tls_main.h"
#include "wu_tls_x509.h"
#include "wu_tls_conn.h"
#include "wu_tls_https.h"
#include "wu_socket.h"
#include "wu_log.h"

extern FILE* g_fphttpslog;
extern FILE* g_fplog;
extern SOCKET g_usersocket;
extern SOCKET g_tls_sclt;
extern SOCKET g_listensocket;
extern SOCKET g_listenhttpssocket;
extern HANDLE g_hNewFile_tmp;
extern char* g_sNewFile_tmp;
extern const struct _http_resources http_resources[];
extern HANDLE g_hConsoleOutput;
extern int g_tls_firstsend;
extern CredHandle* g_credHandle;
extern CtxtHandle* g_ctxtHandle;

SecPkgContext_StreamSizes context_sizes;
char* encryptBuffer = NULL;

static void accept_sec_conn(CtxtHandle* ctxtHandle, CredHandle* credHandle, SOCKET s, SOCKET* s_clt, char* ipaddr_httpsclt, COORD cursorPosition[2]);


/*
 * Function description:
 * - Accept TLS connection. Update UI in console and global variables used for TLS connection when connection is established.
 * Arguments:
 * - ctxtHandle: Pointer to CtxtHandle to initialize with Schannel connection context handle established with client.
 * - credHandle: Credential handle used in AcceptSecurityContext() function.
 * - s: Socket to accept connection on.
 * - s_clt: Pointer to socket to store accepted connection socket with established Schannel connection.
 */

static void
accept_sec_conn(CtxtHandle* ctxtHandle, CredHandle* credHandle, SOCKET s, SOCKET* s_clt,
	char* ipaddr_httpsclt, COORD cursorPosition[2]) {
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
	g_tls_sclt = *s_clt;

	if (encryptBuffer != NULL)
		free(encryptBuffer);

	QueryContextAttributes(ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &context_sizes);
	encryptBuffer = malloc(context_sizes.cbHeader + context_sizes.cbMaximumMessage + context_sizes.cbTrailer);

	return;
}


/*
 * Function description:
 * - Create and send "exit app" html page.
 * Arguments:
 * - cursorPosition: Console cursor position where wu writes info or errors.
 * - httpnv: Structure that contains http header name/value pairs.
 * - theme: Theme to be used to create http resource of "exit app" page.
 * - s_user: Client socket to send message to.
 * - bytesent: Pointer to int to update with number of bytes sent to client.
 */

void
https_wu_quit_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, SOCKET s_user, int* bytesent) {
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


/*
 * Function description:
 * - Close sockets, free handles and buffers, close log files and exit process.
 */

void
https_quit_wu(void) {
	if (g_listensocket)
		closesocket(g_listensocket);
	if (g_usersocket)
		closesocket(g_usersocket);
	if (g_listenhttpssocket)
		closesocket(g_listenhttpssocket);
	if (g_tls_sclt)
		closesocket(g_tls_sclt);
	if (g_hConsoleOutput != INVALID_HANDLE_VALUE)
		CloseHandle(g_hConsoleOutput);
	if (g_hNewFile_tmp != INVALID_HANDLE_VALUE) {
		CloseHandle(g_hNewFile_tmp);
		DeleteFileA((LPCSTR)g_sNewFile_tmp);
	}
	if (g_credHandle != NULL && g_ctxtHandle != NULL && g_tls_sclt) {
		tls_shutdown(g_ctxtHandle, g_credHandle, g_tls_sclt);
		DeleteSecurityContext(g_ctxtHandle);
		FreeCredentialHandle(g_credHandle);
	}
	if (g_fplog != NULL)
		fclose(g_fplog);
	if (g_fphttpslog != NULL)
		fclose(g_fphttpslog);
	if (encryptBuffer != NULL)
		free(encryptBuffer);
	
	WSACleanup();

	ExitProcess(TRUE);

	return;
}


/*
 * Function description:
 * - Main HTTPS loop where GET and POST requests are received and handled.
 * Arguments:
 * - prThread: Pointer to struct paramThread that contains thread parameters such as console cursor position and IP address to bind.
 */

DWORD WINAPI wu_tls_loop(struct paramThread* prThread)
{
	BYTE pbEncodedName[128];
	DWORD err;
	int ret;
	SOCKET s_https, s_https_clt;
	CERT_CONTEXT* pCertContext;
	CredHandle credHandle;
	CtxtHandle ctxtHandle;
	HCERTSTORE hCertStore;
	struct http_reqline reqline;
	struct header_nv headernv[HEADER_NV_MAX_SIZE];
	int bytesent = 0;
	char ipaddr_httpsclt[16];
	NCRYPT_KEY_HANDLE hKey = 0;
	NCRYPT_PROV_HANDLE phProvider = 0;
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
		create_certificate(hCertStore, &pCertContext, pbEncodedName, &phProvider, &hKey, prThread->inaddr);
	}

	CertCloseStore(hCertStore, 0);

	ZeroMemory(&credHandle, sizeof(CredHandle));
	if (get_credantials_handle(&credHandle, pCertContext) < 0) {
		CertFreeCertificateContext(pCertContext); if (phProvider != 0) NCryptFreeObject(phProvider); NCryptFreeObject(hKey);
		err = GetLastError();
		show_error_wait_close(&prThread->cursorPosition[0], ERR_MSG_ACQUIRECREDANTIALSHANDLE, NULL, err);
	}

	CertFreeCertificateContext(pCertContext);
	NCryptFreeObject(phProvider);
	NCryptFreeObject(hKey);

	g_credHandle = &credHandle;

	s_https = create_socket((COORD*)&prThread->cursorPosition);
	bind_socket2((COORD*)&prThread->cursorPosition, s_https, prThread->inaddr);

	for (;;) {
		g_tls_sclt = 0;

		accept_sec_conn(&ctxtHandle, &credHandle, s_https, &s_https_clt, ipaddr_httpsclt, prThread->cursorPosition);

		g_ctxtHandle = &ctxtHandle;
		g_tls_sclt = s_https_clt;

	next_req:
		bytesent = 0;
		tls_recv_output = NULL;

		ret = get_https_request(&ctxtHandle, s_https_clt, &tls_recv_output, &tls_recv_output_size, &prThread->cursorPosition[0],
			&reqline, headernv, prThread->inaddr);

		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_https_clt);
			if (tls_recv_output != NULL)
				free(tls_recv_output);
			continue;
		}

		header_offset = ret;

		if (strcmp(reqline.method, "GET") == 0) {
			ret = handle_get_request(&ctxtHandle, s_https_clt, &reqline, headernv, &bytesent, &prThread->cursorPosition[0]);
			if (ret < 0) {
				tls_shutdown(&ctxtHandle, &credHandle, s_https_clt);
				free(tls_recv_output);
				continue;
			}
		}
		else if (strcmp(reqline.method, "POST") == 0) {
			ret = handle_post_request(&ctxtHandle, s_https_clt, &reqline, headernv, &bytesent, tls_recv_output + header_offset,
				(COORD*)&prThread->cursorPosition);
			if (ret < 0) {
				tls_shutdown(&ctxtHandle, &credHandle, s_https_clt);
				free(tls_recv_output);
				continue;
			}
		}

		log_https_request(ipaddr_httpsclt, &reqline, bytesent);
		free(tls_recv_output);

		goto next_req;
	}

}

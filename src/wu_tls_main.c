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
char* decryptBuffer[10];
unsigned int cb_buffer_extra;
char* buffer_extra;

static void
https_wu_quit_response(COORD cursorPosition[2], struct header_nv* httpnv, int* theme, int s_user, int* bytesent) {
	int ires;
	struct http_resource httplocalres;

	check_cookie_theme(httpnv, theme);

	for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

	ZeroMemory(&httplocalres, sizeof(struct http_resource));
	if (create_local_resource(&httplocalres, ires, *theme) != 0) {
		INPUT_RECORD inRec;
		DWORD read;

		cursorPosition->Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	https_serv_resource(&httplocalres, s_user, NULL, bytesent, g_ctxtHandle, *cursorPosition);

	return;
}

static void
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
	for (i = 1; i < 10; i++) {
		if (decryptBuffer[i] != NULL) {
			free(decryptBuffer[i]);
			decryptBuffer[i] = NULL;
		}
	}
	WSACleanup();
	ExitProcess(TRUE);

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
		CertFreeCertificateContext(pCertContext); LocalFree(pbEncodedName); NCryptFreeObject(phProvider); NCryptFreeObject(hKey);
		err = GetLastError();
		write_info_in_console(ERR_MSG_ACQUIRECREDANTIALSHANDLE, NULL, err);
		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}


	s = create_socket(&prThread->cursorPosition);
	bind_socket2(&prThread->cursorPosition, s, prThread->inaddr);

	for (i = 0; i < 10; i++)
		decryptBuffer[i] = NULL;

	decryptBuffer[0] = malloc(2000);
	buffer_extra = NULL;
	cb_buffer_extra = 0;

	ZeroMemory(headernv, HEADER_NV_MAX_SIZE * (HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE));

	for (;;) {

		g_credHandle = g_ctxtHandle = NULL;
		g_tls_sclt = NULL;

		ZeroMemory(ipaddr_httpsclt, 16);

		Sleep(333);

		s_clt = acceptSecure(s, &credHandle, &ctxtHandle, ipaddr_httpsclt);
		if (prThread->cursorPosition[0].Y > prThread->cursorPosition[1].Y + 5) {
			prThread->cursorPosition[0] = prThread->cursorPosition[1];
			clear_txrx_pane(&prThread->cursorPosition[0]);
		}

		SetConsoleCursorPosition(g_hConsoleOutput, prThread->cursorPosition[0]);
		write_info_in_console(INF_MSG_INCOMING_CONNECTION, NULL, 0);
		prThread->cursorPosition[0].Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, prThread->cursorPosition[0]);

		g_credHandle = &credHandle;
		g_ctxtHandle = &ctxtHandle;
		g_tls_sclt = &s_clt;

		if (encryptBuffer != NULL)
			free(encryptBuffer);

		QueryContextAttributes(&ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &context_sizes);
		encryptBuffer = malloc(context_sizes.cbHeader + context_sizes.cbMaximumMessage + context_sizes.cbTrailer);

	next_req:
		bytesent = 0;


		ret = tls_recv(s_clt, &ctxtHandle, &tls_recv_output, &tls_recv_output_size, &prThread->cursorPosition[0]);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}

		ZeroMemory(&reqline, sizeof(struct http_reqline));
		ret = get_request_line(&reqline, tls_recv_output, tls_recv_output_size);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}
		else
			header_offset = ret;

		ZeroMemory(headernv, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		ret = get_header_nv(headernv, tls_recv_output + header_offset, tls_recv_output_size - header_offset, prThread->inaddr);
		if (ret < 0) {
			tls_shutdown(&ctxtHandle, &credHandle, s_clt);
			continue;
		}
		else
			header_offset += ret;

		if (strcmp(reqline.method, "GET") == 0) {
			int ires;
			struct http_resource httplocalres;

			ires = http_match_resource(reqline.resource);
			if (ires < 0) {
				check_cookie_theme(headernv, &theme);

				for (ires = 0; strcmp(http_resources[ires].resource, "erreur_404") != 0; ires++);

				ZeroMemory(&httplocalres, sizeof(struct http_resource));
				if (create_local_resource(&httplocalres, ires, theme) != 0) {
					INPUT_RECORD inRec;
					DWORD read;

					prThread->cursorPosition[0].Y++;
					SetConsoleCursorPosition(g_hConsoleOutput, prThread->cursorPosition[0]);
					write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

					while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
				}

				https_serv_resource(&httplocalres, s_clt, NULL, &bytesent, &ctxtHandle, prThread->cursorPosition[0]);

				// goto err;
			}
			else if (strcmp(reqline.resource + 1, "quit") == 0) {
				https_wu_quit_response(&prThread->cursorPosition[0], headernv, &theme, s_clt, &bytesent);
				https_quit_wu(s_clt);
			}
			else if (strcmp(reqline.resource + 1, "openRep") == 0) {
				show_download_directory();
				strcpy_s(reqline.resource, HTTP_RESSOURCE_MAX_LENGTH, "/index");
				ires = 0;
				goto after_openrep;
			}
			else {
			after_openrep:
				check_cookie_theme(headernv, &theme);

				ZeroMemory(&httplocalres, sizeof(struct http_resource));
				if (create_local_resource(&httplocalres, ires, theme) != 0) {
					INPUT_RECORD inRec;
					DWORD read;

					prThread->cursorPosition[0].Y++;
					SetConsoleCursorPosition(g_hConsoleOutput, prThread->cursorPosition[0]);
					write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

					while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
				}

				https_serv_resource(&httplocalres, s_clt, NULL, &bytesent, &ctxtHandle, prThread->cursorPosition[0]);
			}
		}
		else if (strcmp(reqline.method, "POST") == 0) {
			if (strcmp(reqline.resource, "/theme") == 0) {
				char cookie[48];

				ret = get_theme_param(headernv, tls_recv_output + header_offset, &theme);
				if (ret != 0) {
					tls_shutdown(&ctxtHandle, &credHandle, s_clt);
					continue;
				}

				ZeroMemory(cookie, 48);
				if (theme == 0)
					strcpy_s(cookie, 48, "theme=dark");
				else
					strcpy_s(cookie, 48, "theme=light");

				https_apply_theme(s_clt, &ctxtHandle, cookie, prThread->cursorPosition);
			}
			else if (strcmp(reqline.resource, "/upload") == 0) {
				struct user_stats upstats;

				clear_txrx_pane(&prThread->cursorPosition[0]);

				check_cookie_theme(headernv, &theme);

				ZeroMemory(&upstats, sizeof(struct user_stats));

				ret = tls_receive_file(&prThread->cursorPosition, headernv, s_clt, &upstats, theme, &bytesent, &ctxtHandle);

				prThread->cursorPosition[0].X = prThread->cursorPosition[1].X;

				if (ret < 0) {
					tls_shutdown(&ctxtHandle, &credHandle, s_clt);
					continue;
				}
				else {
					prThread->cursorPosition[0].Y++;
				}
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

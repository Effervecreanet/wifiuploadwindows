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

extern FILE *g_fphttpslog;
extern const struct _http_resources http_resources[];
extern HANDLE g_hConsoleOutput;

DWORD WINAPI wu_tls_loop(struct paramThread *prThread)
{
	NCRYPT_PROV_HANDLE phProvider;
	NCRYPT_KEY_HANDLE hKey;
	CERT_NAME_BLOB SubjectBlob;
	BYTE pbEncodedName[128];
	DWORD cbEncodedName = 128;
	WSADATA wsaData;
	DWORD err, ret;
	int s, s_clt;
	BYTE ipAddr[4];
	BYTE *pbEncodedAltName = NULL;
	DWORD cbEncodedAltName = 0;
	PCCERT_CONTEXT pCertContext;
	CredHandle credHandle;
	CtxtHandle ctxtHandle;
	HCERTSTORE hCertStore;
	struct in_addr inaddr2oct;
	char BufferIn[2048];
	char *p;
	int i;
	struct http_reqline reqline;
	INPUT_RECORD inRec;
	DWORD read;
	struct header_nv headernv[HEADER_NV_MAX_SIZE];
	int bytesent = 0;
	int theme = 0;
	int bytereceived = 0;

	ZeroMemory(ipAddr, 4);
	ZeroMemory(&inaddr2oct, sizeof(struct in_addr));
	memcpy(&inaddr2oct, &prThread->inaddr, sizeof(struct in_addr));
	ipAddr[0] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[1] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[2] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[3] = inaddr2oct.s_addr & 0x000000FF;

	pCertContext = find_mycert_in_store(&hCertStore);
	if (pCertContext == NULL) {
createcert:
		time_t wutime;
		struct tm *tmval;
		char log_timestr[64];
		ZeroMemory(log_timestr, 64);
		time(&wutime);
		tmval = localtime(&wutime);
		strftime(log_timestr, 64, "%d/%b/%Y:%T -0600", tmval);
		fprintf(g_fphttpslog, "%s -- Create certificate or replace existing one\n", log_timestr);
		fflush(g_fphttpslog);
		generate_key(&phProvider, &hKey);
		if (get_cert_name(&SubjectBlob, pbEncodedName, &cbEncodedName) < 0) {
			err = GetLastError();
			write_info_in_console(ERR_MSG_CERTSTRTONAMEA, NULL, err);
			NCryptFreeObject(phProvider);
			NCryptFreeObject(hKey);
 
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}

		pCertContext =  (PCCERT_CONTEXT)create_cert_self_sign(&prThread->cursorPosition, ipAddr, &SubjectBlob, phProvider, hKey);

		if (FALSE == CertAddCertificateContextToStore(hCertStore, pCertContext, CERT_STORE_ADD_REPLACE_EXISTING, NULL)) {
			NCryptFreeObject(phProvider);
			NCryptFreeObject(hKey);
			err = GetLastError();
			write_info_in_console(ERR_MSG_ADDCERT, NULL, err);
		
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			}
	} else {
		SYSTEMTIME sysTimeNow;
		FILETIME ftSysTimeNow;

		GetSystemTime(&sysTimeNow);
		SystemTimeToFileTime(&sysTimeNow, &ftSysTimeNow);
		if (CompareFileTime(&pCertContext->pCertInfo->NotAfter, &ftSysTimeNow) == -1)
			goto createcert;

	}

	CertCloseStore(hCertStore, 0);

	s = create_socket(&prThread->cursorPosition);
	bind_socket2(&prThread->cursorPosition, s,
				 prThread->inaddr);

	ZeroMemory(&credHandle, sizeof(CredHandle));
	if (get_credantials_handle(&credHandle, pCertContext) < 0) {
		CertFreeCertificateContext(pCertContext);
		LocalFree(pbEncodedAltName);
		NCryptFreeObject(phProvider);
		NCryptFreeObject(hKey);
		err = GetLastError();
		write_info_in_console(ERR_MSG_ACQUIRECREDANTIALSHANDLE, NULL, err);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	ZeroMemory(headernv, HEADER_NV_MAX_SIZE * (HEADER_NAME_MAX_SIZE + HEADER_VALUE_MAX_SIZE));

	for (;;) {
		s_clt = acceptSecure(s, &credHandle, &ctxtHandle);

		if (tls_recv(s_clt, &ctxtHandle, BufferIn, &bytereceived))
			continue;

		ZeroMemory(&reqline, sizeof(struct http_reqline));
		ret = get_request_line(&reqline, BufferIn);
		if (ret < 0)
			continue;

		if (strcmp(reqline.version, HTTP_VERSION) != 0) {
			write_info_in_console(ERR_MSG_BADVERSION, NULL, 0);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}

		ret = get_headernv(headernv, BufferIn + ret);
		if (ret < 0)
			continue;

		i = nv_find_name_client(headernv, "Host");
		if (i < 0 || strcmp(headernv[i].value.v, inet_ntoa(prThread->inaddr)) != 0)
			return -1;

		int test;
		for (test = 0; test < HEADER_NV_MAX_SIZE; test++)
			fprintf(g_fphttpslog, "name_%i: %s value_%i: %s\n", test, headernv[test].name.client, test, headernv[test].value.v);
		fflush(g_fphttpslog);
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

				prThread->cursorPosition.Y++;
				SetConsoleCursorPosition(g_hConsoleOutput, prThread->cursorPosition);
				write_info_in_console(ERR_MSG_CANNOT_GET_RESOURCE, NULL, 0);

				while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			  }

			  https_serv_resource(&httplocalres, s_clt, NULL, &bytesent, &ctxtHandle); 

			  // goto err;
		  }





		} else if (strcmp(reqline.method, "POST") == 0) {


		}
		fprintf(g_fphttpslog, "cbBuffer: %i\nmethod: %s\nresource: %s\nversion: %s\nheadernv: %s\n", secBufferIn[i].cbBuffer, reqline.method, reqline.resource, reqline.version, (char*)secBufferIn[i].pvBuffer + ret);
		fprintf(g_fphttpslog, secBufferIn[i].pvBuffer);
		fflush(g_fphttpslog);
	}

	CertFreeCertificateContext(pCertContext);
	LocalFree(pbEncodedAltName);
	NCryptFreeObject(phProvider);
	NCryptFreeObject(hKey);

	WSACleanup();


	return 0;
}


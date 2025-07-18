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
#include "wu_x509_main.h"
#include "wu_x509_cert.h"
#include "wu_x509_conn.h"
#include "wu_x509_http.h"

#pragma comment (lib, "secur32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "ws2_32.lib")


extern FILE *fp_httpslog;

DWORD WINAPI wu_x509_func(struct paramThread *prThread)
{
	NCRYPT_PROV_HANDLE phProvider;
	NCRYPT_KEY_HANDLE hKey;
	CERT_NAME_BLOB SubjectBlob;
	BYTE pbEncodedName[128];
	DWORD cbEncodedName = 128;
	WSADATA wsaData;
	DWORD err;
	int s, s_clt;
	BYTE ipAddr[4];
	BYTE *pbEncodedAltName = NULL;
	DWORD cbEncodedAltName = 0;
	PCCERT_CONTEXT pCertContext;
	CredHandle credHandle;
	CtxtHandle ctxtHandle;
	HCERTSTORE hCertStore;
	struct in_addr inaddr2oct;
	SecBuffer secBufferIn[4];
	char BufferIn[2048];
	SecBufferDesc secBufferDescInput;
	char *p;
	int i;
	struct http_reqline reqline;

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

	pCertContext = findCertInStore(prThread->conScreenBuffer, &hCertStore);
	if (pCertContext == NULL) {
createcert:
		time_t wutime;
		struct tm *tmval;
		char log_timestr[64];
		ZeroMemory(log_timestr, 64);
		time(&wutime);
		tmval = localtime(&wutime);
		strftime(log_timestr, 64, "%d/%b/%Y:%T -0600", tmval);
		fprintf(fp_httpslog, "%s -- Create certificate or replace existing one\n", log_timestr);
		fflush(fp_httpslog);
		genKey(prThread->conScreenBuffer, &phProvider, &hKey);
		if (getCertName(&SubjectBlob, pbEncodedName, &cbEncodedName) < 0) {
			WriteConsoleA_INFO(prThread->conScreenBuffer, ERR_MSG_CERTSTRTONAMEA, GetLastError());
			NCryptFreeObject(phProvider);
			NCryptFreeObject(hKey);

			while (1)
				Sleep(1000);
		}

		pCertContext =  (PCCERT_CONTEXT)createCertSelfSign(prThread->conScreenBuffer, &prThread->cursorPosition, ipAddr, &SubjectBlob, phProvider, hKey);

		if (FALSE == CertAddCertificateContextToStore(hCertStore, pCertContext, CERT_STORE_ADD_REPLACE_EXISTING, NULL)) {
			NCryptFreeObject(phProvider);
			NCryptFreeObject(hKey);
			WriteConsoleA_INFO(prThread->conScreenBuffer, ERR_MSG_ADD_CERT, GetLastError());
		
			while(1)
					Sleep(1000);
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

	s = create_socket(prThread->conScreenBuffer, &prThread->cursorPosition);
	bind_socket2(prThread->conScreenBuffer, &prThread->cursorPosition, s,
				 prThread->inaddr);

	ZeroMemory(&credHandle, sizeof(CredHandle));
	if (getCredHandle(&credHandle, pCertContext) < 0) {
		CertFreeCertificateContext(pCertContext);
		LocalFree(pbEncodedAltName);
		NCryptFreeObject(phProvider);
		NCryptFreeObject(hKey);
		WriteConsoleA_INFO(prThread->conScreenBuffer, ERR_MSG_ACQUIRECREDENTIALSHANDLE, GetLastError());
		while (1)
			Sleep(1000);	
	}

	ZeroMemory(&secBufferDescInput, sizeof(SecBufferDesc));
	secBufferDescInput.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput.cBuffers = 4;
	secBufferDescInput.pBuffers = secBufferIn;

	ZeroMemory(&secBufferIn, sizeof(SecBuffer) * 4);

	for (;;) {
		s_clt = acceptSecure(s, &credHandle, &ctxtHandle);

		ZeroMemory(BufferIn, 2048);
		secBufferIn[0].cbBuffer = recv(s_clt, BufferIn, 2048, 0);
		if (secBufferIn[0].cbBuffer <= 0)
			continue;
		secBufferIn[0].BufferType = SECBUFFER_DATA;
		secBufferIn[0].pvBuffer = BufferIn;
		secBufferIn[1].BufferType = SECBUFFER_EMPTY;
		secBufferIn[2].BufferType = SECBUFFER_EMPTY;
		secBufferIn[3].BufferType = SECBUFFER_EMPTY;
		
		DecryptMessage(&ctxtHandle, &secBufferDescInput, 0 , 0);

		for (i = 0; i < 4; i++)
			if (secBufferIn[i].BufferType == SECBUFFER_DATA)
				break;

		p = secBufferIn[i].pvBuffer;
		*(p + secBufferIn[i].cbBuffer) = '\0';

		ZeroMemory(&reqline, sizeof(struct http_reqline));
		if (get_reqline(&reqline, secBufferIn[i].pvBuffer) < 0)
			continue;
		fprintf(fp_httpslog, "cbBuffer: %i\nmethod: %s\nresource: %s\nversion: %s\n", secBufferIn[i].cbBuffer, reqline.method, reqline.resource, reqline.version);
		fprintf(fp_httpslog, secBufferIn[i].pvBuffer);
		fflush(fp_httpslog);
	}

	CertFreeCertificateContext(pCertContext);
	LocalFree(pbEncodedAltName);
	NCryptFreeObject(phProvider);
	NCryptFreeObject(hKey);

	WSACleanup();


	return 0;
}


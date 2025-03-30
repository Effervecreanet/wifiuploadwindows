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

#include "wu_x509_helpers.h"

#pragma comment (lib, "secur32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "ws2_32.lib")


int create_socket(int *s)
{
	*s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*s == INVALID_SOCKET) {
		printf("socket failed: %x\n", WSAGetLastError());
		return -1;
	}

	return 0;
}

int bind_socket(int *s, char *strAddr)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(443);
	sin.sin_addr.s_addr = inet_addr(strAddr);

	if (bind(*s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) != 0)
	{
		printf("bind failed: %x\n", WSAGetLastError());
		return -1;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	HCRYPTPROV hProv = (HCRYPTPROV)NULL;
	HCRYPTKEY hKey = (HCRYPTKEY)NULL;
	CERT_NAME_BLOB SubjectBlob;
	BYTE pbEncodedName[128];
	DWORD cbEncodedName;
	WSADATA wsaData;
	DWORD err;
	int s;
	BYTE ipAddr[4] = {192, 168, 1, 17};
	BYTE *pbEncodedAltName = NULL;
	DWORD cbEncodedAltName = 0;
	PCCERT_CONTEXT pCertContext;
	CredHandle credHandle;

	err = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (err != 0) {
		printf("WSAStartup failed with error: %x\n", err);
		return -1;
	}

	if (create_socket(&s) < 0)
		return -1;

	if (bind_socket(&s, "192.168.1.17") < 0)
		return -1;

	listen(s, 10);

	if (genKey(&hProv, &hKey) < 0)
		return -1;

	if (getCertName(&SubjectBlob, pbEncodedName, &cbEncodedName) < 0) {
		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);
		return -1;
	}

	printf("Ok\n");

	if (encodeObject(ipAddr, pbEncodedAltName, cbEncodedAltName) < 0) {
		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);
		return -1;
	}

	printf("Ok\n");

	pCertContext =  (PCCERT_CONTEXT)createCertSelfSign(&SubjectBlob, hProv, pbEncodedAltName, cbEncodedAltName);

	if (pCertContext == NULL) {
		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);
		return -1;
	}

	printf("Ok\n");

	ZeroMemory(&credHandle, sizeof(CredHandle));
	if (getCredHandle(credHandle, pCertContext) < 0) {
		CertFreeCertificateContext(pCertContext);
		LocalFree(pbEncodedAltName);
		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);
		return -1;
	}

	printf("Ok\n");

	CertFreeCertificateContext(pCertContext);
	LocalFree(pbEncodedAltName);
	CryptDestroyKey(hKey);
	CryptReleaseContext(hProv, 0);

	WSACleanup();


	return 0;
}


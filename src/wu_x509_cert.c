#define _WIN32_WINNT 0x0601

#include <Windows.h>
#include <winternl.h>
#include <ncrypt.h>
#include <stdio.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_msg.h"

#define CERT_SUBJECT "CN=wifiupload_localhost"
#define CERT_STR	 "wifiupload_localhost"

extern FILE *fp_log;

void genKey(HANDLE conScreenBuffer, NCRYPT_PROV_HANDLE *phProvider, NCRYPT_KEY_HANDLE *hKey) {
	LPCWSTR strkeyname = L"wifiupload_key";

	if (NCryptOpenStorageProvider(phProvider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_NCRYPTOPENSTORAGEPROVIDER, GetLastError());
		while(1)
			Sleep(1000);

	}
	/*
	NCryptOpenKey(*phProvider, hKey, L"wifiupload_key", 0,0);
	NCryptDeleteKey(*hKey,0);
*/
	if (NCryptCreatePersistedKey(*phProvider, hKey, BCRYPT_RSA_ALGORITHM, strkeyname, AT_KEYEXCHANGE, NCRYPT_OVERWRITE_KEY_FLAG) != ERROR_SUCCESS) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_NCRYPTCREATEPERSISTEDKEY, GetLastError());
		while (1)
			Sleep(1000);
	}

	if (NCryptFinalizeKey(*hKey, 0) != ERROR_SUCCESS) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_NCRYPTFINALIZEKEY, GetLastError());
		while (1)
			Sleep(1000);
	}
	

	return;
}

int getCertName(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName) {

	if (!CertStrToNameA(X509_ASN_ENCODING, CERT_SUBJECT, CERT_X500_NAME_STR, NULL, pbEncodedName, cbEncodedName, NULL))
		return -1;

	SubjectBlob->pbData = pbEncodedName;
	SubjectBlob->cbData = *cbEncodedName;

	return 0;
}

PCCERT_CONTEXT
findCertInStore(HANDLE conScreenBuffer, HCERTSTORE *hCertStore) {
	PCCERT_CONTEXT pCertContext = NULL;
	char namestr[128];

	*hCertStore = CertOpenSystemStore(0, "MY");
	if (hCertStore == NULL) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CANNOT_OPEN_CERT_STORE, GetLastError());
		while(1)
			Sleep(1000);
		ExitThread(1);
	}

	while(pCertContext = CertEnumCertificatesInStore(*hCertStore, pCertContext)) {
		CertGetNameString(pCertContext, CERT_NAME_RDN_TYPE, 0, NULL, namestr, 128);
		if (strcmp(namestr, CERT_STR) == 0)
			return pCertContext;
	}

	return NULL;
}

PCCERT_CONTEXT createCertSelfSign(HANDLE conScreenBuffer, COORD *cursorPosition, BYTE ipAddr[4], CERT_NAME_BLOB *SubjectBlob, NCRYPT_PROV_HANDLE hProv, NCRYPT_KEY_HANDLE hKey) {
	CERT_EXTENSION Extensions[1];
	CERT_EXTENSIONS CertExtensions;
	PCCERT_CONTEXT pCertContext;
	SYSTEMTIME startTime, endTime;
	BYTE *pbEncodedAltName;
	DWORD cbEncodedAltName;
	CERT_ALT_NAME_INFO AltNameInfo = {0};
	CERT_ALT_NAME_ENTRY AltNameEntries[1];

	AltNameEntries[0].dwAltNameChoice = CERT_ALT_NAME_IP_ADDRESS;
	AltNameEntries[0].IPAddress.cbData = 4;
	AltNameEntries[0].IPAddress.pbData = ipAddr; 

	AltNameInfo.cAltEntry = 1;
	AltNameInfo.rgAltEntry = AltNameEntries;

	if (!CryptEncodeObjectEx(X509_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2, &AltNameInfo, CRYPT_ENCODE_ALLOC_FLAG, NULL,
				&pbEncodedAltName,
				&cbEncodedAltName)) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CRYPTENCODEOBJECTEX_FAILED, GetLastError());

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		while(1)
			Sleep(1000);
	}

	Extensions[0].pszObjId = szOID_SUBJECT_ALT_NAME2;
	Extensions[0].fCritical = TRUE;
	Extensions[0].Value.pbData = pbEncodedAltName;
	Extensions[0].Value.cbData = cbEncodedAltName;

	CertExtensions.cExtension = ARRAYSIZE(Extensions);
	CertExtensions.rgExtension = Extensions;


	ZeroMemory(&startTime, sizeof(SYSTEMTIME));
	ZeroMemory(&endTime, sizeof(SYSTEMTIME));
	GetSystemTime(&startTime);
	memcpy(&endTime, &startTime, sizeof(SYSTEMTIME));
	endTime.wYear++;

	pCertContext = CertCreateSelfSignCertificate(hKey, SubjectBlob, 0, NULL, NULL, &startTime,&endTime, &CertExtensions);
	if (pCertContext == NULL) {
		WriteConsoleA_INFO(conScreenBuffer, ERR_MSG_CREATE_CERT, GetLastError());

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		while (1)
			Sleep(1000);
		ExitThread(1);
	}

	LocalFree(pbEncodedAltName);

	return pCertContext;
}

int getCredHandle(CredHandle *credHandle, PCCERT_CONTEXT pCertContext) {
	SCH_CREDENTIALS schCredentials;

	ZeroMemory(&schCredentials, sizeof(SCH_CREDENTIALS));
	schCredentials.dwVersion = SCH_CREDENTIALS_VERSION;
	schCredentials.dwCredFormat = 0;
	schCredentials.cCreds = 1;
	schCredentials.paCred = &pCertContext;

	if (AcquireCredentialsHandle(NULL, UNISP_NAME, SECPKG_CRED_INBOUND, NULL, &schCredentials, NULL, NULL, credHandle, NULL) != SEC_E_OK)
		return -1;

	return 0;
}

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
#include "wu_main.h"

#define CERT_SUBJECT "CN=wifiupload_localhost"
#define CERT_STR	 "wifiupload_localhost"

extern FILE* fp_log;

void generate_key(NCRYPT_PROV_HANDLE* phProvider, NCRYPT_KEY_HANDLE* hKey) {
	LPCWSTR strkeyname = L"wifiupload_key";
	DWORD err;
	INPUT_RECORD inRec;
	DWORD read;

	if (NCryptOpenStorageProvider(phProvider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_NCRYPTOPENSTORAGEPROVIDER, NULL, err);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}
	/*
	NCryptOpenKey(*phProvider, hKey, L"wifiupload_key", 0,0);
	NCryptDeleteKey(*hKey,0);
*/
	if (NCryptCreatePersistedKey(*phProvider, hKey, BCRYPT_RSA_ALGORITHM, strkeyname, AT_KEYEXCHANGE, NCRYPT_OVERWRITE_KEY_FLAG) != ERROR_SUCCESS) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_NCRYPTCREATEPERSISTEDKEY, NULL, err);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	if (NCryptFinalizeKey(*hKey, 0) != ERROR_SUCCESS) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_NCRYPTFINALIZEKEY, NULL, err);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}


	return;
}

int get_cert_name(CERT_NAME_BLOB* SubjectBlob, BYTE pbEncodedName[128], DWORD* cbEncodedName) {

	if (!CertStrToNameA(X509_ASN_ENCODING, CERT_SUBJECT, CERT_X500_NAME_STR, NULL, pbEncodedName, cbEncodedName, NULL))
		return -1;

	SubjectBlob->pbData = pbEncodedName;
	SubjectBlob->cbData = *cbEncodedName;

	return 0;
}

PCCERT_CONTEXT
find_mycert_in_store(HCERTSTORE* hCertStore) {
	PCCERT_CONTEXT pCertContext = NULL;
	char namestr[128];
	INPUT_RECORD inRec;
	DWORD read;
	ULONG err;

	*hCertStore = CertOpenSystemStore(0, "MY");
	if (hCertStore == NULL) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_CANNOTOPENCERTSTORE, NULL, err);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	while (pCertContext = CertEnumCertificatesInStore(*hCertStore, pCertContext)) {
		CertGetNameString(pCertContext, CERT_NAME_RDN_TYPE, 0, NULL, namestr, 128);
		if (strcmp(namestr, CERT_STR) == 0)
			return pCertContext;
	}

	return NULL;
}

PCCERT_CONTEXT create_cert_self_sign(COORD* cursorPosition, BYTE ipAddr[4], CERT_NAME_BLOB* SubjectBlob, NCRYPT_PROV_HANDLE hProv, NCRYPT_KEY_HANDLE hKey) {
	CERT_EXTENSION Extensions[1];
	CERT_EXTENSIONS CertExtensions;
	PCCERT_CONTEXT pCertContext;
	SYSTEMTIME startTime, endTime;
	BYTE* pbEncodedAltName;
	DWORD cbEncodedAltName;
	CERT_ALT_NAME_INFO AltNameInfo = { 0 };
	CERT_ALT_NAME_ENTRY AltNameEntries[1];
	DWORD err;
	INPUT_RECORD inRec;
	DWORD read;

	AltNameEntries[0].dwAltNameChoice = CERT_ALT_NAME_IP_ADDRESS;
	AltNameEntries[0].IPAddress.cbData = 4;
	AltNameEntries[0].IPAddress.pbData = ipAddr;

	AltNameInfo.cAltEntry = 1;
	AltNameInfo.rgAltEntry = AltNameEntries;

	if (!CryptEncodeObjectEx(X509_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2, &AltNameInfo, CRYPT_ENCODE_ALLOC_FLAG, NULL,
		&pbEncodedAltName,
		&cbEncodedAltName)) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_CRYPTENCODEOBJECTEX, NULL, err);

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
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

	pCertContext = CertCreateSelfSignCertificate(hKey, SubjectBlob, 0, NULL, NULL, &startTime, &endTime, &CertExtensions);
	if (pCertContext == NULL) {
		err = GetLastError();
		write_info_in_console(ERR_MSG_CREATECERT, NULL, err);

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	LocalFree(pbEncodedAltName);

	return pCertContext;
}

int get_credantials_handle(CredHandle* credHandle, PCCERT_CONTEXT pCertContext) {
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

#define _WIN32_WINNT 0x0601

#include <Windows.h>
#include <winternl.h>
#include <wincrypt.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

int genKey(HCRYPTPROV *hProv, HCRYPTKEY *hKey) {
	if (!CryptAcquireContext(hProv, NULL, NULL, PROV_RSA_FULL, 0)) {
		printf("CryptAcquireContext failed: %x\n", GetLastError());
		return -1;
	}

	if (!CryptGenKey(*hProv, AT_KEYEXCHANGE, 0x08000000 | CRYPT_EXPORTABLE, hKey)) {
		printf("CryptGenKey failed: %x\n", GetLastError());
		CryptReleaseContext(*hProv, 0);
		return -1;
	}


	return 0;
}

int getCertName(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName) {
	const char *szSubject = "CN=localhost";

	if (!CertStrToNameA(X509_ASN_ENCODING, szSubject, CERT_X500_NAME_STR, NULL, pbEncodedName, cbEncodedName, NULL)) {
		printf("CertStrToNameA failed: %x\n", GetLastError());
		return -1;
	}

	SubjectBlob->pbData = pbEncodedName;
	SubjectBlob->cbData = *cbEncodedName;



	return 0;
}

int encodeObject(BYTE ipAddr[4], BYTE *pbEncodedAltName, DWORD cbEncodedAltName) {
	CERT_ALT_NAME_INFO AltNameInfo = {0};
	CERT_ALT_NAME_ENTRY AltNameEntries[2];

	AltNameEntries[1];

	AltNameEntries[0].dwAltNameChoice = CERT_ALT_NAME_IP_ADDRESS;
	AltNameEntries[0].IPAddress.cbData = sizeof(ipAddr);
	AltNameEntries[0].IPAddress.pbData = (BYTE*)ipAddr;

	AltNameInfo.cAltEntry = 1;
	AltNameInfo.rgAltEntry = AltNameEntries;

	if (!CryptEncodeObjectEx(X509_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2, &AltNameInfo, CRYPT_ENCODE_ALLOC_FLAG, NULL,
				&pbEncodedAltName,
				&cbEncodedAltName)) {
		printf("CryptEncodeObjectEx failed: %x\n", GetLastError());
		return -1;
	}



	return 0;
}


PCCERT_CONTEXT createCertSelfSign(CERT_NAME_BLOB *SubjectBlob, HCRYPTPROV hProv, BYTE *pbEncodedAltName, DWORD cbEncodedAltName) {
	CERT_EXTENSION Extensions[1];
	CERT_EXTENSIONS CertExtensions;
	PCCERT_CONTEXT pCertContext;
	SYSTEMTIME startTime, endTime;

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
	pCertContext = CertCreateSelfSignCertificate(hProv, SubjectBlob, 0, NULL, NULL, &startTime,&endTime, &CertExtensions);
	if (pCertContext == NULL) {
		printf("CertCreateSelfSignCertificate failed: %x", GetLastError());
		return NULL;
	}

	return pCertContext;
}

int getCredHandle(CredHandle credHandle, PCCERT_CONTEXT pCertContext) {
	SCH_CREDENTIALS schCredentials;

	ZeroMemory(&schCredentials, sizeof(SCH_CREDENTIALS));
	schCredentials.dwVersion = SCH_CREDENTIALS_VERSION;
	schCredentials.dwCredFormat = 0;
	schCredentials.cCreds = 1;
	schCredentials.paCred = &pCertContext;

	if (AcquireCredentialsHandle(NULL, UNISP_NAME, SECPKG_CRED_INBOUND, NULL, &schCredentials, NULL, NULL, &credHandle, NULL) != SEC_E_OK) {
		printf("AcquireCredentialsHandle failed: %x\n", GetLastError());
		return -1;
	}

	return 0;
}

int acceptSecure(int s, CredHandle credHandle) {
	int s_clt;
	struct sockaddr_in sin_clt;
	int sinclt_len = sizeof(struct sockaddr_in);
	CtxHandle ctxNewHandle, ctxNewHandle2;

	ZeroMemory(&ctxNewHandle, sizeof(CtxHandle));
	ZeroMemory(&ctxNewHandle2, sizeof(CtxHandle));

	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	ULONG contextAttr = 0;
	TimeStamp tsExpiry;










	return s_clt;
}

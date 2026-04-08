#define _WIN32_WINNT 0x0601

#include <Windows.h>
#include <winternl.h>
#include <ncrypt.h>
#include <stdio.h>
#include <time.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_msg.h"
#include "wu_main.h"

#define CERT_SUBJECT "CN=wifiupload_localhost"
#define CERT_STR	 "wifiupload_localhost"

extern FILE* fp_log;
extern FILE* g_fphttpslog;

static void generate_key(NCRYPT_PROV_HANDLE* phProvider, NCRYPT_KEY_HANDLE* hKey);
static int get_cert_name(CERT_NAME_BLOB* SubjectBlob, BYTE pbEncodedName[128], DWORD* cbEncodedName);
static PCCERT_CONTEXT create_cert_self_sign(BYTE ipAddr[4], CERT_NAME_BLOB* SubjectBlob, NCRYPT_PROV_HANDLE hProv, NCRYPT_KEY_HANDLE hKey);
static void inaddr2octaddr(BYTE ipAddr[4], struct in_addr inaddr2oct);


/*
 * Function description:
 * - Generate a RSA key pair to use in a self-signed certificate.
 * Arguments:
 * - phProvider: Pointer to NCRYPT_PROV_HANDLE to update with handle of provider where key is generated.
 * - hKey: RSA key pair.
 */

static void
generate_key(NCRYPT_PROV_HANDLE* phProvider, NCRYPT_KEY_HANDLE* hKey) {
	LPCWSTR strkeyname = L"wifiupload_key";

	if (NCryptOpenStorageProvider(phProvider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) {
		write_info_in_console(ERR_MSG_NCRYPTOPENSTORAGEPROVIDER, NULL, GetLastError());
		for (;;) Sleep(10000);
	}
	/*
	NCryptOpenKey(*phProvider, hKey, L"wifiupload_key", 0,0);
	NCryptDeleteKey(*hKey,0);
*/
	if (NCryptCreatePersistedKey(*phProvider, hKey, BCRYPT_RSA_ALGORITHM, strkeyname, AT_KEYEXCHANGE, NCRYPT_OVERWRITE_KEY_FLAG) != ERROR_SUCCESS) {
		write_info_in_console(ERR_MSG_NCRYPTCREATEPERSISTEDKEY, NULL, GetLastError());
		for (;;) Sleep(10000);
	}

	if (NCryptFinalizeKey(*hKey, 0) != ERROR_SUCCESS) {
		write_info_in_console(ERR_MSG_NCRYPTFINALIZEKEY, NULL, GetLastError());
		for (;;) Sleep(10000);
	}

	return;
}


/*
 * Function description:
 * - Get encoded subject name to use in certificate creation.
 * Arguments:
 * - subjectBlob: Pointer to CERT_NAME_BLOB to update with encoded subject name.
 * - pbEncodedName: Buffer to store encoded subject name.
 * - cbEncodedName: Pointer to DWORD to update with size of encoded subject name.
 */

static int
get_cert_name(CERT_NAME_BLOB* SubjectBlob, BYTE pbEncodedName[128], DWORD* cbEncodedName) {

	if (!CertStrToNameA(X509_ASN_ENCODING, CERT_SUBJECT, CERT_X500_NAME_STR, NULL, pbEncodedName, cbEncodedName, NULL))
		return -1;

	SubjectBlob->pbData = pbEncodedName;
	SubjectBlob->cbData = *cbEncodedName;

	return 0;
}


/*
 * Function description:
 * - Search for a previously created certificate with CERT_STR in "MY" system certificate store.
 * Arguments:
 * - Certificate store handle to search in.
 * Return value:
 * - Handle of found certificate or NULL if certificate not found.
 */

PCCERT_CONTEXT
find_mycert_in_store(HCERTSTORE* hCertStore) {
	PCCERT_CONTEXT pCertContext = NULL;
	char namestr[128];

	*hCertStore = CertOpenSystemStore(0, "MY");
	if (hCertStore == NULL) {
		write_info_in_console(ERR_MSG_CANNOTOPENCERTSTORE, NULL, GetLastError());
		for (;;) Sleep(10000);
	}

	for (;;) {
		pCertContext = CertEnumCertificatesInStore(*hCertStore, pCertContext);

		if (pCertContext == NULL)
			break;

		CertGetNameString(pCertContext, CERT_NAME_RDN_TYPE, 0, NULL, namestr, 128);
		if (strcmp(namestr, CERT_STR) == 0)
			return pCertContext;
	}

	return NULL;
}


/*
 * Function description:
 * - Gather certificate information such as subject name and subject alternative name, create a self-signed
 *   certificate with this information and return it.
 * Arguments:
 * - ipAddr: IP address to put in subject alternative name extension of certificate.
 * - SubjectBlob: Pointer to CERT_NAME_BLOB that contains encoded subject name.
 * - hProv: Handle of provider where key is generated. It is used in certificate creation.
 * - hKey: Handle of key to use in certificate creation.
 * Return value:
 * - Handle of created certificate.
 */

static PCCERT_CONTEXT
create_cert_self_sign(BYTE ipAddr[4], CERT_NAME_BLOB* SubjectBlob, NCRYPT_PROV_HANDLE hProv, NCRYPT_KEY_HANDLE hKey) {
	CERT_EXTENSION Extensions[1];
	CERT_EXTENSIONS CertExtensions;
	PCCERT_CONTEXT pCertContext;
	SYSTEMTIME startTime, endTime;
	BYTE* pbEncodedAltName;
	DWORD cbEncodedAltName;
	CERT_ALT_NAME_INFO AltNameInfo = { 0 };
	CERT_ALT_NAME_ENTRY AltNameEntries[1];

	AltNameEntries[0].dwAltNameChoice = CERT_ALT_NAME_IP_ADDRESS;
	AltNameEntries[0].IPAddress.cbData = 4;
	AltNameEntries[0].IPAddress.pbData = ipAddr;

	AltNameInfo.cAltEntry = 1;
	AltNameInfo.rgAltEntry = AltNameEntries;

	if (!CryptEncodeObjectEx(X509_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2, &AltNameInfo, CRYPT_ENCODE_ALLOC_FLAG, NULL,
		&pbEncodedAltName,
		&cbEncodedAltName)) {
		write_info_in_console(ERR_MSG_CRYPTENCODEOBJECTEX, NULL, GetLastError());

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		for (;;) Sleep(10000);
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
		write_info_in_console(ERR_MSG_CREATECERT, NULL, GetLastError());

		CryptDestroyKey(hKey);
		CryptReleaseContext(hProv, 0);

		for (;;) Sleep(10000);
	}

	LocalFree(pbEncodedAltName);

	return pCertContext;
}


/*
 * Function description:
 * - Acquire a handle of credentials to use in schannel functions such as AcceptSecurityContext().
 * Arguments:
 * - credHandle: Pointer to CredHandle to update with handle of credentials.
 * - pCertContext: Handle of certificate to use in credentials.
 * Return value:
 * - 0: Success
 * - -1: Failure. It can be caused by AcquireCredentialsHandle() failure.
 */

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


/*
 * Function description:
 * - Check if certificate is still valid (not expired) by comparing current time with certificate expiration time.
 * Arguments:
 * - pCertContext: Handle of certificate to check.
 * Return value:
 * - 0: Certificate is valid.
 * - -1: Certificate is expired.
 */

int
is_certificate_valid(CERT_CONTEXT* pCertContext) {
	SYSTEMTIME sysTimeNow;
	FILETIME ftSysTimeNow;

	GetSystemTime(&sysTimeNow);
	SystemTimeToFileTime(&sysTimeNow, &ftSysTimeNow);
	if (CompareFileTime(&pCertContext->pCertInfo->NotAfter, &ftSysTimeNow) == -1)
		return 1;

	return 0;
}


/*
 * Function description:
 * - Convert an IPv4 address in struct in_addr format to a BYTE array format to use it in certificate creation.
 * Arguments:
 * - ipAddr: BYTE array to update with IP address octets.
 * - inaddr2oct: struct in_addr that contains IP address to convert.
 */

static void
inaddr2octaddr(BYTE ipAddr[4], struct in_addr inaddr2oct) {
	ZeroMemory(ipAddr, 4);

	ipAddr[0] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[1] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[2] = inaddr2oct.s_addr & 0x000000FF;
	inaddr2oct.s_addr >>= 8;
	ipAddr[3] = inaddr2oct.s_addr & 0x000000FF;

	return;
}


/*
 * Function description:
 * - Create a self-signed certificate with CERT_STR in subject name and client IP address in subject alternative name.
 *   Add this certificate to "MY" system certificate store. Update certificate handle, provider handle and key handle used for certificate creation.
 * Arguments:
 * - hCertStore: Certificate store handle to add certificate in.
 * - pCertContext: Pointer to CERT_CONTEXT* to update with handle of created certificate.
 * - pbEncodedName: Buffer to store encoded subject name to use in certificate creation.
 * - phProvider: Pointer to NCRYPT_PROV_HANDLE to update with handle of provider where key is generated. It is used in certificate creation.
 * - hKey: Pointer to NCRYPT_KEY_HANDLE to update with handle of key to use in certificate creation.
 * - inaddr: Client IP address to put in subject alternative name extension of certificate.
 */

void
create_certificate(HCERTSTORE hCertStore, CERT_CONTEXT** pCertContext, BYTE pbEncodedName[128], NCRYPT_PROV_HANDLE* phProvider, NCRYPT_KEY_HANDLE* hKey, struct in_addr inaddr) {
	CERT_NAME_BLOB SubjectBlob;
	DWORD cbEncodedName = 128;
	time_t wutime;
	struct tm tmval;
	char log_timestr[64];
	BYTE ipAddr[4];

	inaddr2octaddr(ipAddr, inaddr);

	ZeroMemory(log_timestr, 64);
	time(&wutime);
	localtime_s(&tmval, &wutime);
	strftime(log_timestr, 64, "%d/%b/%Y:%T -0600", &tmval);
	fprintf(g_fphttpslog, "%s -- Create certificate or replace existing one\n", log_timestr);
	fflush(g_fphttpslog);
	generate_key(phProvider, hKey);
	if (get_cert_name(&SubjectBlob, pbEncodedName, &cbEncodedName) < 0) {
		write_info_in_console(ERR_MSG_CERTSTRTONAMEA, NULL, GetLastError());
		NCryptFreeObject(*phProvider);
		NCryptFreeObject(*hKey);

		for (;;) Sleep(10000);
	}

	*pCertContext = (CERT_CONTEXT*)create_cert_self_sign(ipAddr, &SubjectBlob, *phProvider, *hKey);

	if (FALSE == CertAddCertificateContextToStore(hCertStore, *pCertContext, CERT_STORE_ADD_REPLACE_EXISTING, NULL)) {
		NCryptFreeObject(*phProvider);
		NCryptFreeObject(*hKey);
		write_info_in_console(ERR_MSG_ADDCERT, NULL, GetLastError());

		for (;;) Sleep(10000);
	}

	return;
}

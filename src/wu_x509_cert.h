
void genKey(HANDLE conScreenBuffer, HCRYPTPROV *hProv, HCRYPTKEY *hKey);
int getCertName(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName);
PCCERT_CONTEXT findCertInStore(HANDLE conScreenBuffer, HCERTSTORE *hCertStore);
PCCERT_CONTEXT createCertSelfSign(HANDLE conScreenBuffer, COORD *cursorPosition, BYTE ipAddr[4], CERT_NAME_BLOB *SubjectBlob, HCRYPTPROV hProv, HCRYPTKEY hKey);

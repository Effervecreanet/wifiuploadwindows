int genKey(HCRYPTPROV *hProv, HCRYPTKEY *hKey);

int getCertName(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName);

PCCERT_CONTEXT createCertSelfSign(BYTE ipAddr[4], CERT_NAME_BLOB *SubjectBlob, HCRYPTPROV hProv);


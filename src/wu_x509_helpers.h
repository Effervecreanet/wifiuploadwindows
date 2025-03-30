int genKey(HCRYPTPROV *hProv, HCRYPTKEY *hKey);

int getCertName(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName);

int encodeObject(BYTE ipAddr[4], BYTE *pbEncodedAltName, DWORD cbEncodedAltName);

PCCERT_CONTEXT createCertSelfSign(CERT_NAME_BLOB *SubjectBlob, HCRYPTPROV hProv, BYTE *pbEncodedAltName, DWORD cbEncodedAltName);

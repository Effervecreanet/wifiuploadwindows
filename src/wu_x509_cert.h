
void generate_key(HCRYPTPROV *hProv, HCRYPTKEY *hKey);
int get_cert_name(CERT_NAME_BLOB *SubjectBlob, BYTE pbEncodedName[128], DWORD *cbEncodedName);
PCCERT_CONTEXT find_mycert_in_store(HCERTSTORE *hCertStore);
PCCERT_CONTEXT create_cert_self_sign(COORD *cursorPosition, BYTE ipAddr[4], CERT_NAME_BLOB *SubjectBlob, HCRYPTPROV hProv, HCRYPTKEY hKey);

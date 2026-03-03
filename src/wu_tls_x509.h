
void create_certificate(COORD cursorPosition[2], HCERTSTORE hCertStore, CERT_CONTEXT** pCertContext, BYTE pbEncodedName[128],
						NCRYPT_PROV_HANDLE* phProvider, NCRYPT_KEY_HANDLE* hKey, struct in_addr inaddr);
int is_certificate_valid(CERT_CONTEXT* pCertContext);
int get_credantials_handle(CredHandle* credHandle, PCCERT_CONTEXT pCertContext);
PCCERT_CONTEXT find_mycert_in_store(HCERTSTORE* hCertStore);

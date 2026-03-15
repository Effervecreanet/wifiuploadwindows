
int tls_send(SOCKET s_clt, CtxtHandle* ctxtHandle, char* message, unsigned int message_size, COORD cursorPosition);
int tls_recv(CtxtHandle* ctxtHandle, SOCKET s, char** output, unsigned int* outlen, COORD* cursorPosition);
void tls_shutdown(CtxtHandle *ctxtHandle, CredHandle *credHandle, SOCKET s_clt);
int acceptSecure(SOCKET s, CredHandle *credHandle, CtxtHandle *ctxtHandle, char ipaddr_httpslog[16]);
